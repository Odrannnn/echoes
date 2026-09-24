cmake_minimum_required(VERSION 3.25)
project(metroid_prime2_port LANGUAGES C CXX)

# Native-PC port scaffold for Metroid Prime 2: Echoes (`G2ME01`), a fork of the
# PrimeDecomp/echoes matching decompilation, built the same way as the Metroid
# Prime port: the decompiled game and engine run directly on the GPU through
# Aurora (MIT) instead of an emulator or a static recompilation.
#
# The decompilation is still early (see PORT_NOTES.md), so today this builds the
# game sources against Aurora's SDK headers, plus the port's REL module runtime
# and its tests. `MP_SDK_HEADERS_ONLY` is the only verified configuration.
#
# Reference implementation for every piece of the port layer:
# ../MetroidPrimePort (same engine lineage, complete and working).

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
include(CTest)

option(MP_SDK_HEADERS_ONLY
    "Compile the decompiled game sources against Aurora's SDK headers without \
linking Aurora. This is the only configuration that works while the Echoes \
decompilation is incomplete, and it is what the shim queue is tracked against."
    ON)

# The REL module test needs a module extracted from a disc the user owns, plus
# the symbol list of the executable it was linked against. Both are optional:
# without them the test reports itself skipped.
set(MP_REL_FIXTURE "" CACHE FILEPATH
    "REL module fixture (e.g. a .rel extracted from an owned disc)")
set(MP_DOL_SYMBOLS "" CACHE FILEPATH
    "DOL symbol list matching MP_REL_FIXTURE (config/<version>/symbols.txt)")

# The decompiled sources are written for a *signed* plain `char`, as PowerPC/MWCC
# and every host the Metroid Prime port has been verified on agree on that. ARM
# defaults `char` to unsigned, which silently changes sentinels such as
# CDSPStreamManager's -1 companion index into 255. Force the verified semantics
# everywhere that compiles game-facing code. MSVC/clang-cl already default to
# signed char.
function(mp_signed_char target)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -fsigned-char)
    endif()
endfunction()

# Decompiled game + engine sources. The file list is explicit: every translation
# unit that links must be listed, including registration-only ones.
include(files.cmake)

# Aurora provides the GameCube SDK compatibility layer (GX, OS, PAD, DVD, CARD,
# VI, SI, THP, mtx) plus the SDL3/WebGPU application layer. Only its headers are
# used until the decompilation can link.
add_library(mp_game OBJECT ${MP_GAME_SOURCES})
mp_signed_char(mp_game)
# Include order is deliberate: `include/` (the decompilation's own headers) is
# searched *after* Aurora, because Echoes' `include/dolphin/` tree is the partial
# SDK decompilation and must not shadow Aurora's complete, PC-aware SDK view.
# `platform/include/dolphin` fills the SDK headers Aurora omits. `include/LZO`
# is on the path project-wide because the bundled codec is included both as
# `<lzo1x.h>` (upstream's own build does the same).
target_include_directories(mp_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/aurora/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/LZO)
# compat.h restores the SDK macros Aurora omits; it is force-included ahead of
# every C++ game translation unit. It is C++-only (it pulls in the game's
# CMemory/IAllocator headers), so it must not reach the few C translation units
# in the list - the bundled LZO codec - which are compiled as C, exactly as the
# matching build does.
if(MSVC)
    target_compile_options(mp_game PRIVATE /W3
        "$<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/platform/compat.h>")
else()
    # -Werror=return-type is what the Metroid Prime port uses, but the Echoes
    # decompilation is only ~7% done: several not-yet-finished functions fall off
    # the end of a non-void function, exactly as MWCC accepted (undefined
    # behaviour). Inventing return values would be decompilation work, not port
    # work, so the diagnostic stays a warning until upstream completes them; see
    # PORT_NOTES.md. Tighten this back to -Werror=return-type when the
    # decompilation is complete.
    target_compile_options(mp_game PRIVATE
        -Wformat -Wno-error=return-type -Wno-narrowing -Wno-multichar -Wno-write-strings
        # The game's allocator API carries the literal "??(??)" call-stack tags
        # from the PowerPC build; they are trigraphs to the host compiler and
        # are intentional, so keep them out of the log.
        -Wno-trigraphs
        "$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/platform/compat.h>")
endif()

if(MP_SDK_HEADERS_ONLY)
    # Aurora sets these on its own targets; the header-only build has to state them.
    target_compile_definitions(mp_game PUBLIC TARGET_PC AURORA)

    # The port's own engine-independent sources: SDK shims and stubs, and the
    # glibc floor. They compile against Aurora's headers alone, which is what
    # keeps them verified while the game cannot link. The engine-facing platform
    # files (entry point, debug UI, textures, prompts, randomizer, smoke, disc
    # resources) need Echoes' game types and are still Metroid-Prime-shaped; see
    # platform/README.md.
    add_library(mp_platform OBJECT
        platform/entry.cpp
        platform/shims.cpp
        platform/sdk_stubs.cpp
        platform/glibc_compat.c)
    target_include_directories(mp_platform PRIVATE
        platform/include
        extern/aurora/include
        extern/musyx/include
        include)
    target_compile_definitions(mp_platform PRIVATE TARGET_PC AURORA)

    # The process entry point. It is compiled but not linked: it builds the OS
    # context and memory system the game's entry takes, and those constructors
    # are still upstream work (see PORT_NOTES.md).
    add_library(mp_port_entry OBJECT platform/main.cpp)
    target_include_directories(mp_port_entry PRIVATE
        platform/include
        extern/aurora/include
        extern/musyx/include
        include)
    target_compile_definitions(mp_port_entry PRIVATE TARGET_PC AURORA)

    # REL module runtime tests. The runtime is port code (see PORT_NOTES.md), so
    # these need neither Aurora's libraries nor a GPU device.
    set(MP_REL_TEST_SOURCES platform/rel.cpp)

    add_executable(port_rel_tests tests/port_rel_synthetic.cpp ${MP_REL_TEST_SOURCES})
    target_include_directories(port_rel_tests PRIVATE platform/include)
    add_test(NAME port_rel_tests COMMAND port_rel_tests)
    set_tests_properties(port_rel_tests PROPERTIES TIMEOUT 30 LABELS port)

    add_executable(port_rel_real_tests tests/port_rel_real.cpp ${MP_REL_TEST_SOURCES})
    target_include_directories(port_rel_real_tests PRIVATE platform/include)
    add_test(NAME port_rel_real_tests
        COMMAND port_rel_real_tests "${MP_REL_FIXTURE}" "${MP_DOL_SYMBOLS}")
    set_tests_properties(port_rel_real_tests PROPERTIES
        TIMEOUT 30 LABELS port SKIP_RETURN_CODE 77)

    add_executable(port_entry_tests tests/port_entry_tests.cpp platform/entry.cpp)
    target_include_directories(port_entry_tests PRIVATE platform/include extern/aurora/include)
    target_compile_definitions(port_entry_tests PRIVATE TARGET_PC)
    add_test(NAME port_entry_tests COMMAND port_entry_tests)
    set_tests_properties(port_entry_tests PROPERTIES TIMEOUT 30 LABELS port)

    message(STATUS
        "MP_SDK_HEADERS_ONLY=ON: building the game sources against Aurora's headers "
        "only (no Aurora, no executable). Set -DMP_SDK_HEADERS_ONLY=OFF once the "
        "decompilation is complete enough to link. See PORT_NOTES.md.")
    return()
endif()

add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)
if(TARGET aurora_mtx)
    target_compile_definitions(aurora_mtx PRIVATE MTX_USE_PS=1)
endif()

# MusyX is the game's audio engine; the vendored fork is portable.
add_subdirectory(extern/musyx EXCLUDE_FROM_ALL)
mp_signed_char(musyx)

target_link_libraries(mp_game PUBLIC
    aurora::core aurora::gx aurora::gd aurora::si aurora::vi aurora::pad
    aurora::mtx aurora::os aurora::dvd aurora::thp aurora::card
    musyx)

# ---------------------------------------------------------------------------
# Everything below is the Metroid Prime port's shape, kept as the target for
# when the decompilation can link. It is NOT yet adapted to Echoes and must be
# reworked: the Echoes entry point, the game-side module manager that drives the
# port's REL runtime (platform/rel.cpp), and the port's own platform sources.
# ---------------------------------------------------------------------------
message(FATAL_ERROR
    "MP_SDK_HEADERS_ONLY=OFF is not implemented yet. The port layer (entry point, "
    "REL/module runtime, platform sources, packaging) has to be adapted from "
    "../MetroidPrimePort first; PORT_NOTES.md tracks it.")
