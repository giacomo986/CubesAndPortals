# core/ is the half of the engine that must build without a GPU: voxels, ECS,
# mapgen, protocol, and the portal mathematics. It never includes raylib.h.
# A CI job builds this target alone; if that ever stops working, the server has
# grown a graphics dependency (DESIGN.md 3.1).
add_library(cnp_core STATIC
    audio/attenuation.cpp
    audio/tone.cpp
    math/mat4.cpp
    diag/profiler.cpp
    media/gltf.cpp
    voxel/mesh_pool.cpp
    voxel/pathfind.cpp
    diag/stack_dump.cpp
    ecs/drop.cpp
    ecs/health.cpp
    ecs/lua_ecs.cpp
    ecs/machine.cpp
    ui/panel.cpp
    ecs/mob.cpp
    item/container.cpp
    item/craft_grid.cpp
    item/item_codec.cpp
    item/inventory.cpp
    item/item_registry.cpp
    item/recipe.cpp
    media/content_hash.cpp
    os.cpp
    paths.cpp
    world/modifiers.cpp
    media/media_cache.cpp
    media/face_tiles.cpp
    media/media_set.cpp
    media/png.cpp
    media/texture_pack.cpp
    media/tile_bake.cpp
    math/frustum.cpp
    net/bytes.cpp
    net/crypto.cpp
    net/interest.cpp
    net/protocol.cpp
    net/session.cpp
    portal/portal_clip.cpp
    portal/portal_math.cpp
    portal/portal_traverse.cpp
    portal/portal_tuning.cpp
    registry/core_nodes.cpp
    registry/model_registry.cpp
    registry/node_registry.cpp
    voxel/chunk.cpp
    voxel/find.cpp
    voxel/mesher.cpp
    voxel/light.cpp
    voxel/raycast.cpp
    physics/aabb.cpp
    physics/fall.cpp
    physics/movement.cpp
    physics/player.cpp
    physics/vehicle.cpp
    physics/craft.cpp
    track/piece.cpp
    world/columns.cpp
    world/daylight.cpp
    world/node_meta.cpp
    worldgen/biome.cpp
    worldgen/noise.cpp
    worldgen/spawn.cpp
    worldgen/worldgen.cpp
)

target_include_directories(cnp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# cgltf is PRIVATE on purpose: media/gltf.h hands back this engine's own shapes
# and nothing above it ever sees a cgltf type. DESIGN.md 8 puts the original engine's .b3d
# models in M11, and a second reader has to be able to fill the same structs.
target_include_directories(cnp_core PRIVATE ${CMAKE_SOURCE_DIR}/third_party/cgltf)

# cgltf's implementation is one translation unit's worth of C99 compiled as C++,
# and it is not ours to hold to our warnings.
set_source_files_properties(media/gltf.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter")
# The watchdog in diag/stack_dump.cpp is a thread rather than a timer -- see the
# comment there, and TODO.md OPEN-6: a repeating alarm breaks any blocking wait
# the process is in, which is how the instrument came to hide the fault.
find_package(Threads REQUIRED)
target_link_libraries(cnp_core PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(cnp_core PUBLIC bcrypt ws2_32 dbghelp)   # random bytes, sockets, flecs' stack
endif()
target_link_libraries(cnp_core PRIVATE cnp_warnings)

# Crypto moved down here when players got identities. It owns no socket -- the
# key exchange is bytes in and bytes out, and the only I/O is reading a key file
# -- so the rule "cnp_core builds without a GPU and without a network stack"
# still holds. What moved with it is the reason: the session state machine now
# has to CHECK a signature, and a handshake decision that lives above the layer
# it decides for is a decision no test without a socket can reach.
target_link_libraries(cnp_core PUBLIC cnp_monocypher)

# And the ECS. PUBLIC because the component types are part of what a caller
# reads out of a session -- a header that hands back a flecs::entity and a link
# line that does not mention flecs is a header nobody can use.
target_link_libraries(cnp_core PUBLIC cnp_flecs)

# And Jolt, for vehicles. PRIVATE because nothing above core/physics/vehicle.cpp
# sees a Jolt type: the header hands back a position and an attitude.
target_link_libraries(cnp_core PRIVATE Jolt)

# GCC 13.3 at -O3 miscompiles vehicle.cpp's build_ground: its straight-line
# vectoriser and PRE together hand each block with a shape in a row along x the
# corners of the one before it, lose the last, and build the first from
# whatever is on the stack -- now and then a NaN, and a kart that falls through
# the road (the track's bay is a row of parked karts; a server's copy and its
# client disagreed at getting in one start in five, 2026-09-21). -O2, or -O3
# without either pass, builds it right; the vectoriser is the one given up.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # craft.cpp's build_ground is the same loop.
    set_source_files_properties(physics/vehicle.cpp physics/craft.cpp PROPERTIES
                                COMPILE_OPTIONS -fno-tree-slp-vectorize)
endif()

# The transport is a separate library on purpose. Everything cnp_core knows
# about the network -- the wire format, the version rules, the session state
# machine -- is tested by a binary that links no socket library at all, and the
# split is what keeps that true: move a decision down into the ENet wrapper and
# the protocol test stops being able to reach it.
add_library(cnp_net STATIC
    net/chunk_codec.cpp
    net/client_session.cpp
    net/transport.cpp
    storage/column_keeper.cpp
    storage/world_db.cpp
)

# ENet's CMakeLists predates target_include_directories, so its include path is
# named here rather than inherited from the target.
target_include_directories(cnp_net PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/enet/include
    ${CMAKE_SOURCE_DIR}/third_party/zstd/lib)
target_link_libraries(cnp_net PUBLIC cnp_core enet libzstd_static cnp_sqlite cnp_monocypher)
target_link_libraries(cnp_net PRIVATE cnp_warnings)
