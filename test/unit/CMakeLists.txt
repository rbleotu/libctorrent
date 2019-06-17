add_executable (test_vector test_vector.c)
target_link_libraries (test_vector ctorrent)
add_test (
    NAME test_vector
    COMMAND $<TARGET_FILE:test_vector>
)

add_executable (test_net test_net.c)
target_link_libraries (test_net ctorrent)
add_test (
    NAME test_net
    COMMAND $<TARGET_FILE:test_net>
)

add_executable (test_peer test_peer.c)
target_link_libraries (test_peer ctorrent)
add_test (
    NAME test_peer
    COMMAND $<TARGET_FILE:test_peer>
)

add_executable (test_bitset test_bitset.c)
target_link_libraries (test_bitset ctorrent)
add_test (
    NAME test_bitset
    COMMAND $<TARGET_FILE:test_bitset>
)

add_executable (test_timer test_timer.c)
target_link_libraries (test_timer ctorrent)
add_test (
    NAME test_timer
    COMMAND $<TARGET_FILE:test_timer>
)

add_executable (test_disk test_disk.c)
target_link_libraries (test_disk ctorrent)
add_test (
    NAME test_disk
    COMMAND $<TARGET_FILE:test_disk>
)
