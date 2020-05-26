# CMakeList.txt : CMake project for CMakeProject1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("CMakeProject1")

# Add source to this project's executable.
add_executable (CMakeProject1 "CMakeProject1.cpp" "CMakeProject1.h")
add_executable (CMakeProject2 "CMakeProject1.cpp" "CMakeProject1.h")
add_executable (CMakeProject3 "CMakeProject1.cpp" "CMakeProject1.h")

# TODO: Add tests and install targets if needed.
