# The Luau backend of script.h. LuaJIT joins it in M11 for the original engine's
# compatibility layer (DESIGN.md 5.3); the split exists so that stays a
# link-time choice rather than a rewrite.
add_library(cnp_script STATIC
    script_luau.cpp
    mod_host.cpp
    event_bus.cpp
    world_code.cpp
)

# The generated glue is included by script_luau.cpp, so it has to exist first.
add_dependencies(cnp_script cnp_bindings)

target_include_directories(cnp_script PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
# cnp_modpack and not cnp_core: a mod that arrives as one file is unpacked by
# load_dir, and this layer still knows nothing about a world (see mod_host.h on
# why src/script stays free of core/).
target_link_libraries(cnp_script
    PRIVATE Luau.Compiler cnp_warnings
    PUBLIC  Luau.VM cnp_modpack_lib)
