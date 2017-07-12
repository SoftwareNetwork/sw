#
# cppan
#

################################################################################
#
# tests
#
################################################################################

add_test(NAME run1
    COMMAND ${CMAKE_COMMAND}
        -DCOPY_FROM=${CMAKE_CURRENT_SOURCE_DIR}/1/cppan.yml
        -DCOPY_TO=${CMAKE_CURRENT_BINARY_DIR}/cppan.yml
        -DCPPAN_COMMAND=$<TARGET_FILE:client>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/1/test.cmake
)

################################################################################
