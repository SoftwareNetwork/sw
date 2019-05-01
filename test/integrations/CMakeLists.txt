cmake_minimum_required(VERSION 3.14)

# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake Targets")

project(my C CXX)

set(SW_BUILD_SHARED_LIBS 1)
find_package(SW REQUIRED)
sw_add_package(
    org.sw.demo.sqlite3
    org.sw.demo.glennrp.png
)
sw_execute()

add_executable(png png.cpp)
target_link_libraries(png
    org.sw.demo.glennrp.png
)

add_executable(sqlite3 sqlite3.cpp)
target_link_libraries(sqlite3
    org.sw.demo.sqlite3
)
