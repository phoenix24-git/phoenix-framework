// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#include "phoenix/ecs/world.h"

#include <algorithm>

#include "core/os/memory.h"

namespace phoenix::ecs {

namespace {

constexpr std::size_t align_up(std::size_t v, std::size_t a) {
    return (v + a - 1) & ~(a - 1);
}

std::uint64_t hash_types(const LocalVector<ComponentId>& types) {
    std::uint64_t h = 1469598103934665603ull;  // FNV-1a
    for (ComponentId id : types) {
        h ^= id;
        h *= 1099511628211ull;
    }
    return h;
}

void sort_unique(LocalVector<ComponentId>& types) {
    ComponentId* b = types.ptr();
    ComponentId* e = b + types.size();
    std::sort(b, e);
    types.resize(static_cast<std::uint32_t>(std::unique(b, e) - b));
}

bool same(const LocalVector<ComponentId>& a, const LocalVector<ComponentId>& b) {
    if (a.size() != b.size())
        return false;
    for (std::uint32_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

}  // namespace

// --- Archetype --------------------------------------------------------------------------------

Archetype::Archetype(LocalVector<ComponentId> types, std::uint32_t index)
    : types_(std::move(types)), index_(index) {
    const ComponentRegistry& reg = ComponentRegistry::instance();
    for (ComponentId id : types_) {
        infos_.push_back(&reg.info(id));
        sizes_.push_back(reg.info(id).size);
    }
    // Largest N such that [Entity × N] + each column (aligned) fits in one chunk.
    auto layout = [&](std::uint32_t n, LocalVector<std::uint32_t>* offsets) {
        std::size_t off = sizeof(Entity) * n;
        for (std::uint32_t i = 0; i < types_.size(); ++i) {
            if (sizes_[i] == 0) {
                if (offsets)
                    offsets->push_back(0);
                continue;
            }
            off = align_up(off, infos_[i]->align);
            if (offsets)
                offsets->push_back(static_cast<std::uint32_t>(off));
            off += std::size_t(sizes_[i]) * n;
        }
        return off;
    };
    std::size_t row_bytes = sizeof(Entity);
    for (std::uint32_t s : sizes_)
        row_bytes += s;
    std::uint32_t n = static_cast<std::uint32_t>(kChunkBytes / row_bytes);
    while (n > 1 && layout(n, nullptr) > kChunkBytes)
        --n;
    capacity_ = n < 1 ? 1 : n;  // oversized components still get one per chunk
    layout(capacity_, &offsets_);
}

Archetype::~Archetype() {
    for (Chunk* c : chunks_) {
        for (std::uint32_t row = 0; row < c->count; ++row) {
            for (std::uint32_t col = 0; col < types_.size(); ++col) {
                if (sizes_[col] != 0)
                    infos_[col]->destroy(element(*c, int(col), row));
            }
        }
        Memory::free_aligned_static(c->data);
        delete c;
    }
}

std::size_t Archetype::entity_count() const {
    std::size_t n = 0;
    for (const Chunk* c : chunks_)
        n += c->count;
    return n;
}

std::pair<Chunk*, std::uint32_t> Archetype::allocate_row(Entity e) {
    Chunk* target = nullptr;
    for (Chunk* c : chunks_) {
        if (c->count < capacity_) {
            target = c;
            break;
        }
    }
    if (!target) {
        target = new Chunk();
        target->archetype = this;
        std::size_t needed = sizeof(Entity) * capacity_;
        for (std::uint32_t i = 0; i < types_.size(); ++i) {
            if (sizes_[i] == 0)
                continue;
            needed = align_up(needed, infos_[i]->align) + std::size_t(sizes_[i]) * capacity_;
        }
        const std::size_t bytes = align_up(std::max(kChunkBytes, needed), 64);
        target->data = static_cast<std::byte*>(Memory::alloc_aligned_static(bytes, 64));
        target->versions.resize(types_.size());
        for (std::uint32_t i = 0; i < types_.size(); ++i)
            target->versions[i] = 0;
        chunks_.push_back(target);
    }
    const std::uint32_t row = target->count++;
    target->entities()[row] = e;
    return {target, row};
}

Entity Archetype::swap_remove(Chunk& c, std::uint32_t row, bool destroy_components) {
    const std::uint32_t last = c.count - 1;
    for (std::uint32_t col = 0; col < types_.size(); ++col) {
        if (sizes_[col] == 0)
            continue;
        void* dst = element(c, int(col), row);
        if (destroy_components)
            infos_[col]->destroy(dst);
        if (row != last) {
            void* src = element(c, int(col), last);
            infos_[col]->move_construct(dst, src);
            infos_[col]->destroy(src);
        }
    }
    Entity moved = Entity::null();
    if (row != last) {
        moved = c.entities()[last];
        c.entities()[row] = moved;
    }
    --c.count;
    return moved;
}

// --- World ------------------------------------------------------------------------------------

World::World() {
    empty_ = find_or_create({});
}

World::~World() {
    for (Archetype* a : archetypes_)
        delete a;
}

Archetype* World::find_or_create(LocalVector<ComponentId> types) {
    sort_unique(types);
    const std::uint64_t h = hash_types(types);
    LocalVector<Archetype*>& bucket = by_hash_[h];
    for (Archetype* a : bucket) {
        if (same(a->types(), types))
            return a;
    }
    Archetype* a = new Archetype(std::move(types), archetypes_.size());
    archetypes_.push_back(a);
    bucket.push_back(a);
    return a;
}

Archetype* World::with_component(Archetype* from, ComponentId id) {
    if (Archetype** cached = from->add_edges_.getptr(id))
        return *cached;
    LocalVector<ComponentId> types;
    for (ComponentId t : from->types())
        types.push_back(t);
    types.push_back(id);
    Archetype* to = find_or_create(std::move(types));
    from->add_edges_[id] = to;
    to->remove_edges_[id] = from;
    return to;
}

Archetype* World::without_component(Archetype* from, ComponentId id) {
    if (Archetype** cached = from->remove_edges_.getptr(id))
        return *cached;
    LocalVector<ComponentId> types;
    for (ComponentId t : from->types()) {
        if (t != id)
            types.push_back(t);
    }
    Archetype* to = find_or_create(std::move(types));
    from->remove_edges_[id] = to;
    to->add_edges_[id] = from;
    return to;
}

Entity World::create() {
    assert(!iterating() && "structural change during iteration: use a CommandBuffer");
    std::uint32_t index;
    if (!free_.is_empty()) {
        index = free_[free_.size() - 1];
        free_.resize(free_.size() - 1);
    } else {
        index = records_.size();
        records_.push_back(Record{});
    }
    Record& r = records_[index];
    const Entity e{index, r.generation};
    auto [chunk, row] = empty_->allocate_row(e);
    r.archetype = empty_;
    r.chunk = chunk;
    r.row = row;
    ++alive_count_;
    return e;
}

void World::destroy(Entity e) {
    assert(!iterating() && "structural change during iteration: use a CommandBuffer");
    if (!alive(e))
        return;
    Record& r = records_[e.index];
    const Entity moved = r.archetype->swap_remove(*r.chunk, r.row, /*destroy_components=*/true);
    if (moved.valid()) {
        records_[moved.index].row = r.row;
    }
    r.archetype = nullptr;
    r.chunk = nullptr;
    ++r.generation;
    free_.push_back(e.index);
    --alive_count_;
}

void World::move_entity(Entity e, Archetype* to) {
    Record& r = records_[e.index];
    Archetype* from = r.archetype;
    if (from == to)
        return;
    Chunk* old_chunk = r.chunk;
    const std::uint32_t old_row = r.row;
    auto [chunk, row] = to->allocate_row(e);
    // Move shared columns; destroy columns the destination lacks.
    for (std::uint32_t col = 0; col < from->types_.size(); ++col) {
        if (from->sizes_[col] == 0)
            continue;
        void* src = from->element(*old_chunk, int(col), old_row);
        const int dst_col = to->column_of(from->types_[col]);
        if (dst_col >= 0) {
            from->infos_[col]->move_construct(to->element(*chunk, dst_col, row), src);
            chunk->versions[std::uint32_t(dst_col)] =
                std::max(chunk->versions[std::uint32_t(dst_col)], old_chunk->versions[col]);
        }
        from->infos_[col]->destroy(src);
    }
    // The source row is now destroyed: fill the hole with the chunk's last row.
    const std::uint32_t last = old_chunk->count - 1;
    if (old_row != last) {
        for (std::uint32_t col = 0; col < from->types_.size(); ++col) {
            if (from->sizes_[col] == 0)
                continue;
            void* dst = from->element(*old_chunk, int(col), old_row);
            void* src = from->element(*old_chunk, int(col), last);
            from->infos_[col]->move_construct(dst, src);
            from->infos_[col]->destroy(src);
        }
        const Entity moved = old_chunk->entities()[last];
        old_chunk->entities()[old_row] = moved;
        records_[moved.index].row = old_row;
    }
    --old_chunk->count;
    r.archetype = to;
    r.chunk = chunk;
    r.row = row;
}

bool World::has_component(Entity e, ComponentId id) const {
    return alive(e) && records_[e.index].archetype->has(id);
}

void* World::get_component(Entity e, ComponentId id, bool mark_changed) {
    if (!alive(e))
        return nullptr;
    const Record& r = records_[e.index];
    const int col = r.archetype->column_of(id);
    if (col < 0)
        return nullptr;
    if (mark_changed)
        r.chunk->versions[std::uint32_t(col)] = tick_;
    return r.archetype->element(*r.chunk, col, r.row);
}

void* World::add_component(Entity e, ComponentId id) {
    if (has_component(e, id))
        return get_component(e, id, true);
    assert(!iterating() && "structural change during iteration: use a CommandBuffer");
    move_entity(e, with_component(records_[e.index].archetype, id));
    const Record& r = records_[e.index];
    const int col = r.archetype->column_of(id);
    r.chunk->versions[std::uint32_t(col)] = tick_;
    void* storage = r.archetype->element(*r.chunk, col, r.row);
    const ComponentInfo& info = ComponentRegistry::instance().info(id);
    if (info.size != 0)
        info.construct(storage);
    return storage;
}

void World::remove_component(Entity e, ComponentId id) {
    if (!has_component(e, id))
        return;
    assert(!iterating() && "structural change during iteration: use a CommandBuffer");
    move_entity(e, without_component(records_[e.index].archetype, id));
}

std::span<const ComponentId> World::components_of(Entity e) const {
    if (!alive(e))
        return {};
    const LocalVector<ComponentId>& types = records_[e.index].archetype->types();
    return {types.ptr(), types.size()};
}

}  // namespace phoenix::ecs
