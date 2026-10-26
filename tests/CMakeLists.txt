# The fundamental tests only. Each one guards something that breaks silently
# and costs a lot when it does: input from the network, a player's saved world,
# a mod escaping its sandbox, two machines disagreeing about the world, or the
# portal transform itself. Everything else is checked by playing the game.
#
# Headless unless labelled "gpu": `ctest -LE gpu` runs on a machine with no
# display.

# cnp_unit_test(<name> LIBS <libraries...> [ARGS <arguments...>])
function(cnp_unit_test name)
    cmake_parse_arguments(T "" "" "LIBS;ARGS" ${ARGN})
    add_executable(cnp_${name}_test ${name}_test.cpp)
    target_link_libraries(cnp_${name}_test PRIVATE ${T_LIBS} cnp_warnings)
    add_test(NAME ${name} COMMAND cnp_${name}_test ${T_ARGS})
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
endfunction()

# What the parsers do with truncated, corrupted and random input from a peer.
cnp_unit_test(protocol    LIBS cnp_core)
cnp_unit_test(chunk_codec LIBS cnp_net)
target_include_directories(cnp_chunk_codec_test PRIVATE ${CMAKE_SOURCE_DIR}/third_party/zstd/lib)

# Session encryption: tampering, replay, reflected frames, impostors.
cnp_unit_test(crypto LIBS cnp_net)

# A saved world loads back the same, and an older file still opens.
cnp_unit_test(storage   LIBS cnp_net)
cnp_unit_test(migration LIBS cnp_net)

# The portal arithmetic, the one rule for "is this box inside a block", and the
# server's copy of a kart coming out where the client's is, to the last bit.
cnp_unit_test(portal_math     LIBS cnp_core)
cnp_unit_test(portal_traverse LIBS cnp_core)
cnp_unit_test(physics_box     LIBS cnp_core)

# One VM per mod, capabilities, and a runaway mod that dies alone. The timeout
# matters: if the interrupt stops firing this test hangs instead of failing.
cnp_unit_test(script_isolation LIBS cnp_script ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mods)
set_tests_properties(script_isolation PROPERTIES TIMEOUT 60)

# The same seed makes the same world, with the real mods loaded.
cnp_unit_test(worldgen LIBS cnp_core cnp_script
              ARGS ${CMAKE_SOURCE_DIR}/mods ${CMAKE_CURRENT_SOURCE_DIR}/mods_mapgen)

# The real server binary and a windowless client in another process: the right
# clients join and see the server's world, the wrong ones are refused.
add_executable(cnp_join_probe join_probe.cpp)
target_link_libraries(cnp_join_probe PRIVATE cnp_net cnp_warnings)

add_test(NAME two_process_join
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/two_process_join.sh
                 $<TARGET_FILE:cnp_server> $<TARGET_FILE:cnp_join_probe>)

# A server stopped and started again finds the world, and the chests, it left.
add_test(NAME persistence
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/persistence.sh $<TARGET_FILE:cnp_server>)

set_tests_properties(two_process_join persistence PROPERTIES
                     TIMEOUT 120
                     ENVIRONMENT "CNP_LOG_DIR=${CMAKE_BINARY_DIR}/test-logs")

# What a portal shows matches the world seen from the other side, against a
# viewpoint derived by hand. Needs a window.
add_test(NAME portal_continuity COMMAND cnp_client --verify --watchdog 15)
set_tests_properties(portal_continuity PROPERTIES TIMEOUT 120 LABELS "gpu")
