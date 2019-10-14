cmake_minimum_required(VERSION 3.13)

project(untitled CXX)

set(CMAKE_CXX_STANDARD 14)
set(SW_BUILD_SHARED_LIBS 1)
set(DEPENDENCIES
    org.sw.demo.google.tesseract.libtesseract-master
    org.sw.demo.intel.opencv.imgproc
    org.sw.demo.intel.opencv.highgui
)

find_package(SW REQUIRED)
sw_add_package(${DEPENDENCIES})
sw_execute()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${DEPENDENCIES})
