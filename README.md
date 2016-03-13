# C++ Archive Network

Use wiki to get help with CPPAN.

Use issues to put your feedback, bugs and other stuff.

## Client

https://cppan.org/client/

## Build

    git clone https://github.com/cppan/cppan
    cd cppan
    git submodule update --init
    mkdir build && cd build
    cmake ..
    make -j4

On Windows you should download the latest LibreSSL and put it into 'dep/libressl' directory.
