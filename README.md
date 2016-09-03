# [C++ Archive Network](https://cppan.org/)

Read [documentation](https://github.com/cppan/cppan/blob/master/doc/readme.md) to get help with CPPAN.

Use issues to put your feedback, bugs and other stuff.

## [Client binaries](https://cppan.org/client/)

Binaries depends on gcc-5 ABI on linux and VC2015 redist on Windows.

## Build

    git clone https://github.com/cppan/cppan
    cd cppan
    git submodule update --init
    mkdir build && cd build
    cmake ..
    # for linux
    # cmake .. -DCMAKE_C_COMPILER=gcc-5 -DCMAKE_CXX_COMPILER=g++-5
    make -j4

On Windows you should download the latest LibreSSL and put it into 'dep/libressl' directory before cmake step.
