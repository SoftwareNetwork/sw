# CMakeList.txt : CMake project for CMakeProject1, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("CMakeProject1")

# Add source to this project's executable.
add_executable (CMakeProject1 "CMakeProject1.cpp" "CMakeProject1.h")
add_executable (CMakeProject2 "CMakeProject1.cpp" "CMakeProject1.h")
add_executable (CMakeProject3 "CMakeProject1.cpp" "CMakeProject1.h")
add_executable (CMakeProject4 "CMakeProject2.cpp")
add_executable (CMakeProject5 "CMakeProject3.cpp")

#
set(src lib1.cpp)
add_library(lib1 ${src} lib1.h)

#
add_library(lib2)
target_sources(lib2 PRIVATE lib2.h lib2.cpp)
target_link_libraries(lib2 PRIVATE lib1)

#
add_library(lib3 STATIC lib3.cpp lib3.h)
target_link_libraries(lib3 PRIVATE lib2)

#
add_library(lib4 STATIC lib4.cpp lib4.h)
target_link_libraries(CMakeProject4 PRIVATE lib4)

#
target_link_libraries(CMakeProject5 PRIVATE lib3)
