# The binding generator of DESIGN.md 5.4. It links the Luau that is already a
# submodule, so it is not a new toolchain -- it is thirty lines of host around
# the generator, which is itself written in Luau.
add_executable(cnp_api_gen api_gen/main.cpp)
target_link_libraries(cnp_api_gen PRIVATE Luau.VM Luau.Compiler cnp_warnings)

set(CNP_API_DESC ${CMAKE_SOURCE_DIR}/src/script/api.luau)
set(CNP_API_GEN  ${CMAKE_CURRENT_SOURCE_DIR}/api_gen/gen.luau)

# Generated into the source tree and committed, because generated code you
# cannot read in a diff is generated code nobody reviews. The generator only
# rewrites a file whose content actually changed, so this does not make every
# build rebuild everything downstream.
# A cross build (Windows, macOS from Linux) cannot run the generator it just
# built; it takes the committed files as they are.
if(NOT CMAKE_CROSSCOMPILING)
    add_custom_command(
        OUTPUT  ${CMAKE_SOURCE_DIR}/src/script/api_generated.inc
                ${CMAKE_SOURCE_DIR}/docs/api.d.luau
                ${CMAKE_SOURCE_DIR}/docs/API.md
        COMMAND cnp_api_gen ${CNP_API_DESC} ${CNP_API_GEN} ${CMAKE_SOURCE_DIR}
        DEPENDS cnp_api_gen ${CNP_API_DESC} ${CNP_API_GEN}
                ${CMAKE_SOURCE_DIR}/src/script/script_luau.cpp ${CMAKE_SOURCE_DIR}/docs/MODDING.md
        COMMENT "Generating the scripting bindings from src/script/api.luau"
        VERBATIM)
endif()

add_custom_target(cnp_bindings DEPENDS ${CMAKE_SOURCE_DIR}/src/script/api_generated.inc)

# The placeholder textures of DESIGN.md 4.5. It loads the mods, because the
# blocks worth a texture are the ones a mod registered -- so it links the same
# script host the server does, and nothing graphical: the PNG encoder lives in
# cnp_core precisely so a headless build can produce media.
add_executable(cnp_bake_textures bake_textures/main.cpp)
target_link_libraries(cnp_bake_textures PRIVATE cnp_core cnp_script cnp_warnings)
target_compile_definitions(cnp_bake_textures PRIVATE CNP_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# The link that is not loopback (DESIGN.md 8, M6's exit test). A UDP relay that
# delays, loses and duplicates datagrams between two of our own processes.
#
# It links nothing of the engine on purpose: it must not be able to know what a
# message is. Anything it could parse would be a shortcut past the layer it is
# supposed to be underneath.
add_executable(cnp_netshape netshape/main.cpp)
target_link_libraries(cnp_netshape PRIVATE cnp_warnings)

# The other half of "two servers on different machines" (DESIGN.md 8): two wall
# clocks that do not agree. An LD_PRELOAD shim rather than a container or a VM,
# because Linux time namespaces do not cover CLOCK_REALTIME and a VM would buy a
# kernel nobody asked for. C, and it links nothing of the engine -- it sits
# underneath libc, where the server cannot tell it apart from a wrong clock.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")   # an LD_PRELOAD shim: Linux only
    add_library(cnp_clock_skew SHARED clock_skew/clock_skew.c)
    set_target_properties(cnp_clock_skew PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
    target_compile_options(cnp_clock_skew PRIVATE -Wall -Wextra)
    target_link_libraries(cnp_clock_skew PRIVATE ${CMAKE_DL_LIBS})
endif()

# A mod as one file, and back (DESIGN.md M10). The format is its own library and
# knows nothing about a VM; this links the manifest parser as well, because the
# one thing the format deliberately does not do is read the mod.conf inside to
# name the file -- a name on the outside of a package is a name that can lie.
add_executable(cnp_modpack modpack/main.cpp)
target_link_libraries(cnp_modpack PRIVATE cnp_modpack_lib cnp_script cnp_warnings
                                         cnp_portable)
