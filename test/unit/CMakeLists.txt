add_executable (test_vector test_vector.c)
target_link_libraries (test_vector ctorrent)
add_test (
    NAME test_vector
    COMMAND $<TARGET_FILE:test_vector>
)
