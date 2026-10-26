# Cross-compiling the Windows build from Linux with zig's bundled MinGW
# (OPEN-40): tools/dist/package-windows.sh does the whole thing.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   "${CMAKE_CURRENT_LIST_DIR}/zig-cc")
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/zig-c++")
set(CMAKE_RC_COMPILER  "${CMAKE_CURRENT_LIST_DIR}/zig-rc")
set(CMAKE_AR           "${CMAKE_CURRENT_LIST_DIR}/zig-ar")
set(CMAKE_RANLIB       "${CMAKE_CURRENT_LIST_DIR}/zig-ranlib")
