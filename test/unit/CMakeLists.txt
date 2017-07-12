#
# cppan
#

################################################################################
#
# tests
#
################################################################################

add_executable(source_test source.cpp)
set_property(TARGET source_test PROPERTY FOLDER test)
target_link_libraries(source_test common pvt.cppan.demo.philsquared.catch)
add_test(NAME source COMMAND source_test)

add_executable(string_test string.cpp)
set_property(TARGET string_test PROPERTY FOLDER test)
target_link_libraries(string_test support pvt.cppan.demo.philsquared.catch)
add_test(NAME string COMMAND string_test)

################################################################################
