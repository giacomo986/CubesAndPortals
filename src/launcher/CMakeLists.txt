# The launcher (src/launcher/main.cpp): the game's builds, drawn with the
# client's own title-screen code.
add_executable(cnp_launcher
    main.cpp
    fetch.cpp
    mods.cpp
    ../client/gfx/gfx_raylib.cpp
    ../client/gfx/platform_raylib.cpp
    ../client/ui/hud.cpp
    ../client/ui/lang.cpp
    ../client/ui/title.cpp
)
target_include_directories(cnp_launcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(cnp_launcher PRIVATE cnp_core cnp_script cnp_modpack_lib raylib cnp_warnings cnp_portable)
target_compile_definitions(cnp_launcher PRIVATE CNP_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
add_dependencies(cnp_launcher cnp_client)
if(WIN32)
    target_link_options(cnp_launcher PRIVATE -Wl,--subsystem,windows)
endif()
