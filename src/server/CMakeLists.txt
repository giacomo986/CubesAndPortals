# The headless half of DESIGN.md 3.1: no window, no GL, no raylib. It links
# cnp_core and cnp_script and nothing else, which is the check -- if this stops
# linking, the server has grown a graphics dependency.
add_executable(cnp_server main.cpp admin.cpp)
target_link_libraries(cnp_server PRIVATE cnp_core cnp_net cnp_script cnp_warnings
                                          cnp_portable)
target_compile_definitions(cnp_server PRIVATE CNP_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# So a crash leaves a stack somebody can read. backtrace_symbols_fd prints what
# the dynamic symbol table holds, and without this that table has almost nothing
# in it for a static binary: the stack comes out as a column of addresses.
target_link_options(cnp_server PRIVATE -rdynamic)
