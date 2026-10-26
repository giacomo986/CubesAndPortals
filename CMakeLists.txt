cmake_minimum_required(VERSION 3.25)   # raylib 6.0 asks for 3.25
project(cubes_and_portals LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(CNP_WERROR "Treat our own warnings as errors" OFF)

# A build meant to run on a machine that is not this one (DESIGN.md M10). The
# C++ runtime goes INTO the binaries, because libstdc++ is the one library a
# distribution cannot assume: a program built against a newer one dies at
# startup on an older machine with a message about a GLIBCXX version, and
# nothing about that message says which program or which library.
#
# What stays dynamic is the system's: libc, and the GL and X11 the driver owns.
# Linking those statically is not portability, it is a program carrying its own
# graphics driver.
option(CNP_PORTABLE "Link the C++ runtime into the binaries, for a build that travels" OFF)

add_library(cnp_portable INTERFACE)
if(CNP_PORTABLE)
    target_link_options(cnp_portable INTERFACE -static-libstdc++ -static-libgcc)
endif()

# ---------------------------------------------------------------------------
# third_party
# ---------------------------------------------------------------------------
# raylib is pinned to 6.0. GL 3.3 core is the floor the design targets: it is
# what gl_ClipDistance needs, and staying on it keeps the GLES/Web fallback
# (DESIGN.md 2.4, "strada B") a shader #ifdef rather than a port.
set(PLATFORM "Desktop" CACHE STRING "" FORCE)
set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/raylib EXCLUDE_FROM_ALL)

# ENet 1.3.18. UDP with reliable and unreliable channels, fragmentation,
# sequencing and congestion control, in 6.5k lines of C with no dependencies of
# its own (DESIGN.md 3.2). Pinned to a release tag, not to master.
add_subdirectory(third_party/enet EXCLUDE_FROM_ALL)

# zstd 1.5.7, for chunks on the wire and in the database (DESIGN.md 3.2). Its
# own CMake builds the command line tools and the test suite by default; none of
# that is wanted here, and the legacy decoder for formats from 2015 even less.
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC   ON  CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/zstd/build/cmake EXCLUDE_FROM_ALL)

# SQLite 3.50.4, the amalgamation, compiled here rather than as a subproject
# because it is one C file. See third_party/sqlite/README.md for why this one is
# a copy and the others are submodules.
#
# The defines are the ones worth having off: no shared-cache (deprecated and a
# footgun), no deprecated APIs, and no loadable extensions -- a database file is
# something a server may one day be handed, and an extension load is arbitrary
# code execution from a file.
add_library(cnp_sqlite STATIC third_party/sqlite/sqlite3.c)
target_include_directories(cnp_sqlite PUBLIC third_party/sqlite)
target_compile_definitions(cnp_sqlite PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_OMIT_DEPRECATED=1
    SQLITE_OMIT_SHARED_CACHE=1
    SQLITE_DQS=0
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_ENABLE_MATH_FUNCTIONS=1)
target_compile_options(cnp_sqlite PRIVATE -w)   # not our code; not our warnings

# Monocypher 4.0.3: X25519, XChaCha20-Poly1305, BLAKE2b and EdDSA, in two files
# of C99 with no dependencies and no build system of its own to inherit.
#
# DESIGN.md 3.2 names libsodium, and this is a deliberate departure from it,
# recorded here rather than in a commit nobody rereads. The reason is the
# constraint behind that line rather than the line itself: libsodium builds with
# autotools, and adding it as a submodule would put autoconf and libtool on the
# list of things somebody needs installed to compile this game -- exactly the
# kind of toolchain growth DESIGN.md 3.1 rules out. Monocypher offers the same
# primitives the design asks for, including the ed25519 that M6's signed tickets
# will need, and compiles as two source files.
#
# What it costs: libsodium has had far more eyes and more formal review.
# Monocypher's answer to that is a test suite verified against TIS-Interpreter
# and a set of vectors from libsodium itself -- good, but not the same thing.
# The swap back is a build change and not a protocol change, because nothing
# above core/net/crypto.h knows which library is underneath.
add_library(cnp_monocypher STATIC
    third_party/monocypher/src/monocypher.c
    third_party/monocypher/src/optional/monocypher-ed25519.c)
target_include_directories(cnp_monocypher PUBLIC
    third_party/monocypher/src
    third_party/monocypher/src/optional)
target_compile_options(cnp_monocypher PRIVATE -w)   # not our code; not our warnings

# Flecs 4.1.6: the ECS DESIGN.md 6.1 names, vendored the way sqlite and
# monocypher are -- from its own single-file distribution rather than through its
# build system.
#
# Why not add_subdirectory: flecs builds a shared library by default, installs
# targets, and carries a test suite and an examples tree. None of that is wanted
# here, and inheriting a build system to switch all of it off is how a
# dependency starts deciding things about this one. distr/flecs.c is the
# amalgamation the project ships for exactly this, and it is two files.
#
# FLECS_CUSTOM_BUILD with nothing enabled would cut it down further; it is left
# whole on purpose for now, because the addons this will want -- queries,
# hierarchies, and the REST explorer DESIGN.md 6.1 asks for -- are in there, and
# trimming before knowing which is a saving measured against a guess.
add_library(cnp_flecs STATIC third_party/flecs/distr/flecs.c)
target_include_directories(cnp_flecs PUBLIC third_party/flecs/distr)
# flecs.h defines flecs_STATIC itself when nothing asks for a shared build.
target_compile_options(cnp_flecs PRIVATE -w)   # not our code; not our warnings

set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/luau EXCLUDE_FROM_ALL)

# Jolt Physics 5.6.0, for the bodies that are vehicles (core/physics/vehicle.h):
# wheels, suspension and tyres that grip and slide, which the player's own box
# resolver has no business growing. The player stays on that resolver -- it is
# the same rule the server validates with.
#
# Deterministic across platforms from the start, because the server will run
# the same vehicle to check a driver's input (TODO.md OPEN-47), and a server
# whose kart ends a lap somewhere else than the client's corrects every lap.
# No AVX, FMA, F16C or the bit-count instructions: a CNP_PORTABLE build has to
# start on a machine that lacks them, and the determinism turns FMA off anyway.
# Jolt's own flags stay in its own directory; OVERRIDE_CXX_FLAGS would have set
# ours too. No debug renderer, profiler, object streams, compute backends or
# install rules: nothing here draws Jolt, profiles it or saves it.
set(CROSS_PLATFORM_DETERMINISTIC        ON  CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS                  OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION        OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED   OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS                 OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM                OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL                      OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE       OFF CACHE BOOL "" FORCE)
foreach(opt USE_AVX USE_AVX2 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD
            JPH_USE_DX12 JPH_USE_VK JPH_USE_MTL JPH_USE_CPU_COMPUTE)
    set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory(third_party/JoltPhysics/Build EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# our code
# ---------------------------------------------------------------------------
add_library(cnp_warnings INTERFACE)
target_compile_options(cnp_warnings INTERFACE
    -Wall -Wextra -Wshadow -Wno-unused-parameter
    $<$<BOOL:${CNP_WERROR}>:-Werror>)

add_subdirectory(tools)
add_subdirectory(src/core)
add_subdirectory(src/modpack)
add_subdirectory(src/script)
add_subdirectory(src/server)
add_subdirectory(src/client)

enable_testing()
add_subdirectory(tests)
