cmake_minimum_required(VERSION 3.13)

project(tess_example CXX)

set(CMAKE_CXX_STANDARD 20)
set(SW_BUILD_SHARED_LIBS 1)
set(DEPENDENCIES
    org.sw.demo.google.tesseract.libtesseract
    org.sw.demo.intel.opencv.imgproc
    org.sw.demo.intel.opencv.highgui
)

find_package(SW REQUIRED)
sw_add_package(${DEPENDENCIES})
sw_execute()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${DEPENDENCIES})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
