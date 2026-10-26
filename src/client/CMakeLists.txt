add_executable(cnp_client
    main.cpp
    gfx/gfx_raylib.cpp
    gfx/platform_raylib.cpp
    snd/snd_raylib.cpp
    render/world_render.cpp
    render/body_render.cpp
    render/fx_render.cpp
    render/model_render.cpp
    render/model_store.cpp
    render/mod_pass.cpp
    render/portal_render.cpp
    render/selection.cpp
    ui/hud.cpp
    ui/lang.cpp
    update_check.cpp
)

target_include_directories(cnp_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(cnp_client PRIVATE cnp_net cnp_core cnp_script raylib cnp_warnings
                                         cnp_portable)

# Where the files are when this build is NOT standing in the source tree is
# core/paths.h's question now; this is the last place it looks, and it is what
# makes a build tree work without a flag.
target_compile_definitions(cnp_client PRIVATE
    CNP_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# The stack dumper prints what the dynamic symbol table holds, so the symbols
# have to be in it. Without this a hang reports a column of addresses and the
# whole point -- reading the stack without a debugger -- is lost.
target_link_options(cnp_client PRIVATE -rdynamic)

# The title screen's worlds are served by cnp_server, found beside this program
# or in the build tree's src/server; building the client builds it too.
add_dependencies(cnp_client cnp_server)

# A window and no console beside it on Windows; stdout goes nowhere there.
# The flag and not WIN32_EXECUTABLE, which zig's MinGW link line ignores.
if(WIN32)
    target_link_options(cnp_client PRIVATE -Wl,--subsystem,windows)
endif()
