# C++ Archive Network

https://cppan.org/

See [docs](https://github.com/cppan/cppan/tree/master/doc) to get help with CPPAN.

Use issues to put your feedback, bugs and other stuff.

## Client binaries

https://cppan.org/client/

Binaries depends on gcc-5 ABI on linux and VC2015 redist on Windows.

## Build

    git clone https://github.com/cppan/cppan
    cd cppan
    git submodule update --init
    mkdir build && cd build
    cmake ..
    make -j4

On Windows you should download the latest LibreSSL and put it into 'dep/libressl' directory before cmake step.
