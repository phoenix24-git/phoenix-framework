# Builds the Phoenix foundation layer: upstream Godot Engine core/ (MIT, vendored verbatim in
# ../phoenix_core) as the static library
# `phoenix::foundation`, the foundation layer of Phoenix (Variant, Object/ClassDB, RID, String,
# containers, math, io, resources).
#
#   ../phoenix_core/       verbatim upstream copy; update with dev/tools/vendor_foundation.py, never edit
#   codegen/        runs Godot's own *.gen.* generators without SCons
#   compat/         minimal headers for the few scene/servers/main symbols core/ references,
#                   the seam where Phoenix's servers plug in
#   platform/       OS / FileAccess / DirAccess for Phoenix (SDL3 + POSIX/Win32)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(FOUNDATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../phoenix_core")
set(FOUNDATION_GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(FOUNDATION_TP "${FOUNDATION_DIR}/thirdparty")

# --- Generated sources ---------------------------------------------------------------------
set(FOUNDATION_GENERATED
    "${FOUNDATION_GEN}/core/disabled_classes.gen.h"
    "${FOUNDATION_GEN}/core/version_generated.gen.h"
    "${FOUNDATION_GEN}/core/version_hash.gen.cpp"
    "${FOUNDATION_GEN}/core/script_encryption_key.gen.cpp"
    "${FOUNDATION_GEN}/core/authors.gen.h"
    "${FOUNDATION_GEN}/core/donors.gen.h"
    "${FOUNDATION_GEN}/core/license.gen.h"
    "${FOUNDATION_GEN}/core/object/gdvirtual.gen.h"
    "${FOUNDATION_GEN}/core/extension/ext_wrappers.gen.h"
    "${FOUNDATION_GEN}/core/extension/gdextension_interface.gen.h"
    "${FOUNDATION_GEN}/core/extension/gdextension_interface_dump.gen.h"
    "${FOUNDATION_GEN}/core/input/default_controller_mappings.gen.cpp"
    "${FOUNDATION_GEN}/core/profiling/profiling.gen.h"
    "${FOUNDATION_GEN}/modules/modules_enabled.gen.h")
file(GLOB_RECURSE FOUNDATION_GENERATOR_DEPS CONFIGURE_DEPENDS
     "${FOUNDATION_DIR}/core/*.py" "${FOUNDATION_DIR}/core/extension/gdextension_interface.json"
     "${CMAKE_CURRENT_SOURCE_DIR}/codegen/*.py")
add_custom_command(
    OUTPUT ${FOUNDATION_GENERATED}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/codegen/generate.py"
            "${FOUNDATION_DIR}" "${FOUNDATION_GEN}"
    DEPENDS ${FOUNDATION_GENERATOR_DEPS} "${FOUNDATION_DIR}/UPSTREAM"
    COMMENT "Generating Phoenix foundation sources"
    VERBATIM)

# --- Third-party code used by core/ (lists mirror core/SCsub) ------------------------------
set(_tp_c
    misc/fastlz.c misc/r128.c misc/smaz.c
    zlib/adler32.c zlib/compress.c zlib/crc32.c zlib/deflate.c zlib/inffast.c zlib/inflate.c
    zlib/inftrees.c zlib/trees.c zlib/uncompr.c zlib/zutil.c
    minizip/ioapi.c minizip/unzip.c minizip/zip.c
    brotli/common/constants.c brotli/common/context.c brotli/common/dictionary.c
    brotli/common/platform.c brotli/common/shared_dictionary.c brotli/common/transform.c
    brotli/dec/bit_reader.c brotli/dec/decode.c brotli/dec/huffman.c brotli/dec/prefix.c
    brotli/dec/state.c brotli/dec/static_init.c
    zstd/common/debug.c zstd/common/entropy_common.c zstd/common/error_private.c
    zstd/common/fse_decompress.c zstd/common/pool.c zstd/common/threading.c zstd/common/xxhash.c
    zstd/common/zstd_common.c zstd/compress/fse_compress.c zstd/compress/hist.c
    zstd/compress/huf_compress.c zstd/compress/zstd_compress.c zstd/compress/zstd_double_fast.c
    zstd/compress/zstd_fast.c zstd/compress/zstd_lazy.c zstd/compress/zstd_ldm.c
    zstd/compress/zstd_opt.c zstd/compress/zstd_preSplit.c zstd/compress/zstdmt_compress.c
    zstd/compress/zstd_compress_literals.c zstd/compress/zstd_compress_sequences.c
    zstd/compress/zstd_compress_superblock.c zstd/decompress/huf_decompress.c
    zstd/decompress/zstd_ddict.c zstd/decompress/zstd_decompress_block.c
    zstd/decompress/zstd_decompress.c
    mbedtls/library/aes.c mbedtls/library/base64.c mbedtls/library/constant_time.c
    mbedtls/library/ctr_drbg.c mbedtls/library/entropy.c mbedtls/library/md.c
    mbedtls/library/md5.c mbedtls/library/sha1.c mbedtls/library/sha256.c
    mbedtls/library/godot_core_mbedtls_platform.c)
set(_tp_cxx
    misc/pcg.cpp misc/polypartition.cpp misc/smolv.cpp
    clipper2/src/clipper.engine.cpp clipper2/src/clipper.offset.cpp clipper2/src/clipper.rectclip.cpp)
list(TRANSFORM _tp_c PREPEND "${FOUNDATION_TP}/")
list(TRANSFORM _tp_cxx PREPEND "${FOUNDATION_TP}/")

file(GLOB_RECURSE FOUNDATION_SOURCES CONFIGURE_DEPENDS "${FOUNDATION_DIR}/core/*.cpp")
file(GLOB FOUNDATION_GLUE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/compat/*.cpp")

add_library(phoenix_foundation STATIC
    ${FOUNDATION_SOURCES} ${_tp_c} ${_tp_cxx} ${FOUNDATION_GLUE_SOURCES}
    "${FOUNDATION_GEN}/core/version_hash.gen.cpp"
    "${FOUNDATION_GEN}/core/script_encryption_key.gen.cpp"
    "${FOUNDATION_GEN}/core/input/default_controller_mappings.gen.cpp"
    ${FOUNDATION_GENERATED})
add_library(phoenix::foundation ALIAS phoenix_foundation)

# Godot's own language level and conventions (see Godot's SConstruct).
set_target_properties(phoenix_foundation PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
                      CXX_EXTENSIONS ON C_STANDARD 17 POSITION_INDEPENDENT_CODE ON)
target_include_directories(phoenix_foundation
    PUBLIC "${FOUNDATION_DIR}" "${FOUNDATION_GEN}" "${CMAKE_CURRENT_SOURCE_DIR}/compat"
           "${CMAKE_CURRENT_SOURCE_DIR}/platform"
           # Generated headers that Godot includes by bare name from their source directory:
           "${FOUNDATION_GEN}/core" "${FOUNDATION_GEN}/core/object" "${FOUNDATION_GEN}/core/extension"
           "${FOUNDATION_GEN}/core/profiling"
           # core/io/compression.h includes <zlib.h>/<zstd.h>: always Godot's bundled copies.
           "${FOUNDATION_TP}/zlib" "${FOUNDATION_TP}/zstd"
    PRIVATE "${FOUNDATION_TP}/brotli/include"
            "${FOUNDATION_TP}/clipper2/include" "${FOUNDATION_TP}/mbedtls/include" "${FOUNDATION_TP}/misc")
target_compile_definitions(phoenix_foundation
    PUBLIC
        THREADS_ENABLED MINIZIP_ENABLED BROTLI_ENABLED CLIPPER2_ENABLED ZSTD_STATIC_LINKING_ONLY
        PHOENIX_FOUNDATION
        $<$<CONFIG:Debug>:DEBUG_ENABLED>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    PRIVATE
        "MBEDTLS_CONFIG_FILE=\"thirdparty/mbedtls/include/godot_core_mbedtls_config.h\""
        BROTLI_BUILD_PORTABLE
        ZSTD_DISABLE_ASM)  # Godot adds huf_decompress_amd64.S; the portable C path is used here
if(EMSCRIPTEN)
    target_compile_definitions(phoenix_foundation PUBLIC WEB_ENABLED UNIX_ENABLED UNIX_SOCKET_UNAVAILABLE)
elseif(WIN32)
    target_compile_definitions(phoenix_foundation PUBLIC WINDOWS_ENABLED NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    target_compile_definitions(phoenix_foundation PUBLIC MACOS_ENABLED UNIX_ENABLED)
else()
    target_compile_definitions(phoenix_foundation PUBLIC LINUXBSD_ENABLED UNIX_ENABLED)
endif()
if(MSVC)
    target_compile_options(phoenix_foundation PRIVATE /W0 /utf-8 /bigobj)
else()
    target_compile_options(phoenix_foundation PRIVATE -w $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
endif()
# Godot checks that sanitizer builds announce themselves (core/typedefs.h).
set(_phx_san_flags "${CMAKE_CXX_FLAGS} ${PHOENIX_SANITIZE}")
foreach(_san IN ITEMS thread:TSAN address:ASAN undefined:UBSAN memory:MSAN leak:LSAN)
    string(REPLACE ":" ";" _pair "${_san}")
    list(GET _pair 0 _flag)
    list(GET _pair 1 _define)
    if(_phx_san_flags MATCHES "sanitize=[a-z,]*${_flag}" OR PHOENIX_SANITIZE MATCHES "${_flag}")
        target_compile_definitions(phoenix_foundation PUBLIC ${_define}_ENABLED)
    endif()
endforeach()
if(PHOENIX_SANITIZE AND NOT MSVC AND NOT EMSCRIPTEN)
    # Same instrumentation as Phoenix's own targets (see phoenix_target_defaults).
    target_compile_options(phoenix_foundation PUBLIC -fsanitize=${PHOENIX_SANITIZE} -fno-omit-frame-pointer)
    target_link_options(phoenix_foundation PUBLIC -fsanitize=${PHOENIX_SANITIZE})
endif()
find_package(Threads REQUIRED)
target_link_libraries(phoenix_foundation PUBLIC SDL3::SDL3-static Threads::Threads)

# --- Upstream core/ unit tests, run against this build -----------------------------------
if(PHOENIX_BUILD_TESTS)
    file(GLOB_RECURSE FOUNDATION_TESTS CONFIGURE_DEPENDS "${FOUNDATION_DIR}/tests/core/*.cpp")
    # Networking needs a platform IP/socket driver (Godot: drivers/unix/ip_unix, net_socket_*),
    # which Phoenix does not provide yet. Tracked in the README's "Known gaps".
    list(FILTER FOUNDATION_TESTS EXCLUDE REGEX
         "/test_(ip|tcp_server|udp_server|uds_server|stream_peer_tcp)\\.cpp$")
    add_executable(phoenix_foundation_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/foundation_test_main.cpp"
        "${FOUNDATION_DIR}/tests/test_macros.cpp" "${FOUNDATION_DIR}/tests/test_utils.cpp"
        "${FOUNDATION_DIR}/tests/signal_watcher.cpp"
        ${FOUNDATION_TESTS})
    target_link_libraries(phoenix_foundation_tests PRIVATE phoenix_foundation)
    target_include_directories(phoenix_foundation_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${FOUNDATION_DIR}/tests")
    target_compile_definitions(phoenix_foundation_tests PRIVATE TESTS_ENABLED
                               PHOENIX_FOUNDATION_DIR="${FOUNDATION_DIR}")
    set_target_properties(phoenix_foundation_tests PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
    if(NOT MSVC)
        target_compile_options(phoenix_foundation_tests PRIVATE -w -fno-exceptions)
    endif()
    if(EMSCRIPTEN)
        target_link_options(phoenix_foundation_tests PRIVATE
            -sPROXY_TO_PTHREAD -sPTHREAD_POOL_SIZE=16 -sEXIT_RUNTIME=1 -sNODERAWFS=1
            -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=8GB -sSTACK_SIZE=4MB -sDEFAULT_PTHREAD_STACK_SIZE=2MB
            -sENVIRONMENT=node,worker -Wno-pthreads-mem-growth
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/../../test/node_pre.js")
    endif()
    add_test(NAME phoenix_foundation_tests COMMAND phoenix_foundation_tests)
    set_tests_properties(phoenix_foundation_tests PROPERTIES TIMEOUT 600
        ENVIRONMENT "TSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tests/tsan.supp")
endif()
