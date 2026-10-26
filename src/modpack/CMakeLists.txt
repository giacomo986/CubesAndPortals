# A mod as one file (DESIGN.md M10). Its own library because the engine reads
# packages and a tool writes them, and neither should have to link the other:
# nothing here knows what a VM, a registry or a world is. Under it are only zstd
# -- already linked for the chunks on the wire -- and monocypher's BLAKE2b,
# which is the hash core/media/content_hash.h uses for a file a world serves.
add_library(cnp_modpack_lib STATIC
    mod_package.cpp
)

target_include_directories(cnp_modpack_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_include_directories(cnp_modpack_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/zstd/lib)
target_link_libraries(cnp_modpack_lib PUBLIC cnp_monocypher PRIVATE libzstd_static cnp_warnings)
