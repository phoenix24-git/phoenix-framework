// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Archetype ECS world.
//
//  * Entities with the same component set share an Archetype. An archetype stores entities in
//    16 KiB Chunks laid out struct-of-arrays: [Entity × N][A × N][B × N]… so systems stream
//    through contiguous memory (cache- and SIMD-friendly, like Unity DOTS chunks).
//  * Adding/removing a component moves the entity to another archetype; edges between
//    archetypes are cached so repeated moves are O(components).
//  * Every chunk column carries a change version: queries can filter `changed<T>()` to touch
//    only chunks written since a given tick (used by render extract, networking, editors).
//  * Structural changes must not happen while iterating; record them in a CommandBuffer.
#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "phoenix/ecs/component.h"
#include "phoenix/ecs/entity.h"

namespace phoenix::ecs {

inline constexpr std::size_t kChunkBytes = 16 * 1024;

class Archetype;

struct Chunk {
    Archetype* archetype = nullptr;
    std::byte* data = nullptr;
    std::uint32_t count = 0;
    LocalVector<std::uint32_t> versions;  ///< last write tick per column

    Entity* entities() const { return reinterpret_cast<Entity*>(data); }
};

class Archetype {
  public:
    Archetype(LocalVector<ComponentId> types, std::uint32_t index);
    ~Archetype();
    Archetype(const Archetype&) = delete;
    Archetype& operator=(const Archetype&) = delete;

    const LocalVector<ComponentId>& types() const { return types_; }
    bool has(ComponentId id) const { return column_of(id) >= 0; }
    /// Column index for a component, or -1. Tag components have a column but no storage.
    int column_of(ComponentId id) const {
        const ComponentId* begin = types_.ptr();
        const ComponentId* end = begin + types_.size();
        const ComponentId* it = std::lower_bound(begin, end, id);
        return (it != end && *it == id) ? int(it - begin) : -1;
    }
    std::uint32_t capacity() const { return capacity_; }
    std::uint32_t index() const { return index_; }
    const LocalVector<Chunk*>& chunks() const { return chunks_; }
    std::size_t entity_count() const;

    void* column_ptr(const Chunk& c, int column) const {
        return c.data + offsets_[std::size_t(column)];
    }
    template <typename T>
    T* column(const Chunk& c, int column) const {
        return static_cast<T*>(column_ptr(c, column));
    }
    void* element(const Chunk& c, int column, std::uint32_t row) const {
        return static_cast<std::byte*>(column_ptr(c, column)) +
               std::size_t(row) * sizes_[std::size_t(column)];
    }
    std::uint32_t column_size(int column) const { return sizes_[std::size_t(column)]; }

  private:
    friend class World;
    /// Appends a slot (components not yet constructed). Returns {chunk, row}.
    std::pair<Chunk*, std::uint32_t> allocate_row(Entity e);
    /// Removes `row` by moving the last row into it; returns the entity that moved (or null).
    Entity swap_remove(Chunk& c, std::uint32_t row, bool destroy_components);

    LocalVector<ComponentId> types_;  // sorted
    LocalVector<std::uint32_t> sizes_;
    LocalVector<std::uint32_t> offsets_;
    LocalVector<const ComponentInfo*> infos_;
    std::uint32_t capacity_ = 0;
    std::uint32_t index_ = 0;
    // LocalVector grows with memrealloc, so it only holds trivially relocatable data:
    // chunks are owned raw pointers (freed in ~Archetype).
    LocalVector<Chunk*> chunks_;
    HashMap<ComponentId, Archetype*> add_edges_;
    HashMap<ComponentId, Archetype*> remove_edges_;
};

template <typename... Ts>
class Query;

class World {
  public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // --- Entities ------------------------------------------------------------------------
    Entity create();
    /// Destroys immediately (not while iterating: use a CommandBuffer).
    void destroy(Entity e);
    bool alive(Entity e) const {
        return e.index < records_.size() && records_[e.index].generation == e.generation &&
               records_[e.index].archetype != nullptr;
    }
    std::size_t entity_count() const { return alive_count_; }

    // --- Components ----------------------------------------------------------------------
    template <typename T, typename... Args>
    T& add(Entity e, Args&&... args);
    template <typename T>
    void remove(Entity e) {
        remove_component(e, component_id<T>());
    }
    template <typename T>
    bool has(Entity e) const {
        return has_component(e, component_id<T>());
    }
    /// Pointer to the component or nullptr. Marks the column as written (mutable access).
    template <typename T>
    T* get(Entity e) {
        return static_cast<T*>(get_component(e, component_id<T>(), true));
    }
    template <typename T>
    const T* get(Entity e) const {
        return static_cast<const T*>(
            const_cast<World*>(this)->get_component(e, component_id<T>(), false));
    }

    bool has_component(Entity e, ComponentId id) const;
    void* get_component(Entity e, ComponentId id, bool mark_changed);
    /// Adds a default-constructed component by id (editor/serialization path); returns storage.
    void* add_component(Entity e, ComponentId id);
    void remove_component(Entity e, ComponentId id);
    /// Component ids on an entity, in archetype (sorted) order.
    std::span<const ComponentId> components_of(Entity e) const;

    // --- Queries ---------------------------------------------------------------------------
    /// `world.query<const LocalTransform, Velocity>()`: const = read, non-const = write.
    template <typename... Ts>
    Query<Ts...> query();

    /// Archetypes created so far (stable pointers; the list only grows).
    const LocalVector<Archetype*>& archetypes() const { return archetypes_; }

    // --- Change detection ------------------------------------------------------------------
    /// Advances the global change tick (the scheduler does this once per system run).
    std::uint32_t advance_tick() { return ++tick_; }
    std::uint32_t tick() const { return tick_; }

    /// Iteration guard: structural changes while iterating are bugs.
    void begin_iteration() { iterating_.fetch_add(1, std::memory_order_relaxed); }
    void end_iteration() { iterating_.fetch_sub(1, std::memory_order_relaxed); }
    bool iterating() const { return iterating_.load(std::memory_order_relaxed) > 0; }

    struct Record {
        Archetype* archetype = nullptr;
        Chunk* chunk = nullptr;
        std::uint32_t row = 0;
        std::uint32_t generation = 0;
    };
    const Record* record(Entity e) const { return alive(e) ? &records_[e.index] : nullptr; }

  private:
    Archetype* find_or_create(LocalVector<ComponentId> types);
    Archetype* with_component(Archetype* from, ComponentId id);
    Archetype* without_component(Archetype* from, ComponentId id);
    /// Moves entity `e` to archetype `to`; constructs nothing for new columns.
    void move_entity(Entity e, Archetype* to);

    LocalVector<Archetype*> archetypes_;  // owned (see ~World)
    HashMap<std::uint64_t, LocalVector<Archetype*>> by_hash_;
    Archetype* empty_ = nullptr;
    LocalVector<Record> records_;
    LocalVector<std::uint32_t> free_;
    std::size_t alive_count_ = 0;
    std::uint32_t tick_ = 1;
    std::atomic<int> iterating_{0};  // parallel systems iterate concurrently
};

template <typename T, typename... Args>
T& World::add(Entity e, Args&&... args) {
    const ComponentId id = component_id<T>();
    void* storage = nullptr;
    if (has_component(e, id)) {
        storage = get_component(e, id, true);
        if constexpr (!std::is_empty_v<T>) {
            *static_cast<T*>(storage) = T(std::forward<Args>(args)...);
        }
    } else {
        assert(!iterating() && "structural change during iteration: use a CommandBuffer");
        assert(alive(e));
        move_entity(e, with_component(records_[e.index].archetype, id));
        const Record& r = records_[e.index];
        const int col = r.archetype->column_of(id);
        r.chunk->versions[std::size_t(col)] = tick_;
        storage = r.archetype->element(*r.chunk, col, r.row);
        if constexpr (!std::is_empty_v<T>) {
            new (storage) T(std::forward<Args>(args)...);
        }
    }
    if constexpr (std::is_empty_v<T>) {
        static T tag{};
        return tag;
    } else {
        return *static_cast<T*>(storage);
    }
}

// ---------------------------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------------------------

/// Typed query over all archetypes that contain every T (plus `with`/`without` filters).
/// `const T` = read-only access; non-const = write access (bumps the chunk's change version).
template <typename... Ts>
class Query {
  public:
    explicit Query(World& world) : world_(world) {
        (required_.push_back(component_id<std::remove_const_t<Ts>>()), ...);
    }

    template <typename... Cs>
    Query& with() {
        (required_.push_back(component_id<Cs>()), ...);
        return *this;
    }
    template <typename... Cs>
    Query& without() {
        (excluded_.push_back(component_id<Cs>()), ...);
        return *this;
    }
    /// Only chunks where component C was written after `since_tick`.
    template <typename C>
    Query& changed(std::uint32_t since_tick) {
        changed_.push_back({component_id<C>(), since_tick});
        return *this;
    }

    /// fn(Ts&...) or fn(Entity, Ts&...)
    template <typename Fn>
    void each(Fn&& fn) {
        for_each_chunk([&](Chunk& c, Entity* entities, Ts*... cols) {
            for (std::uint32_t i = 0; i < c.count; ++i) {
                if constexpr (std::is_invocable_v<Fn&, Entity, Ts&...>) {
                    fn(entities[i], cols[i]...);
                } else {
                    fn(cols[i]...);
                }
            }
        });
    }

    /// fn(Chunk&, Entity*, Ts*...) for SIMD-style loops over whole chunks.
    template <typename Fn>
    void for_each_chunk(Fn&& fn) {
        world_.begin_iteration();
        for (Chunk* c : matching_chunks()) {
            visit(*c, fn);
        }
        world_.end_iteration();
    }

    /// Like each(), but chunks run in parallel on the foundation WorkerThreadPool
    /// (#include "phoenix/ecs/parallel.h").
    template <typename Fn>
    void par_each(Fn&& fn);

    std::size_t count() {
        std::size_t n = 0;
        for (Chunk* c : matching_chunks())
            n += c->count;
        return n;
    }

    LocalVector<Chunk*> matching_chunks() {
        LocalVector<Chunk*> out;
        for (Archetype* a : world_.archetypes()) {
            if (!matches(*a))
                continue;
            for (Chunk* c : a->chunks()) {
                if (c->count == 0 || !chunk_changed(*a, *c))
                    continue;
                out.push_back(c);
            }
        }
        return out;
    }

    bool matches(const Archetype& a) const {
        for (ComponentId id : required_)
            if (!a.has(id))
                return false;
        for (ComponentId id : excluded_)
            if (a.has(id))
                return false;
        return true;
    }

  private:
    bool chunk_changed(const Archetype& a, const Chunk& c) const {
        for (const auto& [id, since] : changed_) {
            const int col = a.column_of(id);
            if (col < 0 || c.versions[std::size_t(col)] <= since)
                return false;
        }
        return true;
    }

    template <typename Fn>
    void visit(Chunk& c, Fn& fn) {
        const Archetype& a = *c.archetype;
        const std::uint32_t tick = world_.tick();
        auto col = [&](auto* tag) {
            using T = std::remove_pointer_t<decltype(tag)>;
            const int index = a.column_of(component_id<std::remove_const_t<T>>());
            if constexpr (!std::is_const_v<T>)
                c.versions[std::size_t(index)] = tick;
            return static_cast<T*>(a.column_ptr(c, index));
        };
        fn(c, c.entities(), col(static_cast<Ts*>(nullptr))...);
    }

    World& world_;
    LocalVector<ComponentId> required_;
    LocalVector<ComponentId> excluded_;
    LocalVector<std::pair<ComponentId, std::uint32_t>> changed_;

    template <typename...>
    friend class Query;
};

template <typename... Ts>
Query<Ts...> World::query() {
    return Query<Ts...>(*this);
}

}  // namespace phoenix::ecs
