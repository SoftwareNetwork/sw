## [C++ Archive Network](https://cppan.org/)

[![Build Status](https://travis-ci.org/cppan/cppan.svg?branch=master)](https://travis-ci.org/cppan/cppan)
[![Build status](https://ci.appveyor.com/api/projects/status/iacnrt6byhy8ox5v?svg=true)](https://ci.appveyor.com/project/egorpugin/cppan)

### Resources

- Homepage: https://cppan.org/
- Docs: https://github.com/cppan/cppan/blob/master/doc/
- Download: https://cppan.org/client/
- Issue tracking: https://github.com/cppan/cppan/issues
- Forum: https://groups.google.com/forum/#!forum/cppan


### Quick Start

1. Download the latest client application, unpack and put it to PATH.
1. (!) Run once `cppan` from any directory without any arguments to perform initial configuration.
1. In your `CMakeLists.txt` write:
```
find_package(CPPAN REQUIRED)
cppan_add_package(
    pvt.cppan.demo.sqlite3
    pvt.cppan.demo.fmt-4
    pvt.cppan.demo.madler.zlib-*
    pvt.cppan.demo.boost.asio-1.66
    ...
    libs you want to add
)
cppan_execute()

# near your target
add_executable(myexe ...)
target_link_libraries(myexe
  pvt.cppan.demo.sqlite3
  pvt.cppan.demo.madler.zlib
  pvt.cppan.demo.boost.asio
  ...
)
```
4. Perform other usual CMake steps.


### Dependencies

- `CMake >= 3.2`

### Download & Install

#### Linux

##### Ubuntu 16.04

```
sudo apt install cmake
wget https://cppan.org/client/cppan-master-Linux-client.deb 
sudo dpkg -i cppan-master-Linux-client.deb
```

##### Ubuntu 14.04

```
wget https://www.cmake.org/files/v3.6/cmake-3.6.1-Linux-x86_64.sh
sudo sh cmake-3.6.1-Linux-x86_64.sh --skip-license --prefix=/usr
sudo add-apt-repository ppa:ubuntu-toolchain-r/test 
sudo apt-get update && sudo apt-get install gcc-5
wget https://cppan.org/client/cppan-master-Linux-client.deb 
sudo dpkg -i cppan-master-Linux-client.deb
```

#### Windows

[Download client](https://cppan.org/client/cppan-master-Windows-client.zip), unzip it and put under PATH.

#### macOS

```
wget https://cppan.org/client/cppan-master-macOS-client.zip 
unzip cppan-master-macOS-client.zip 
sudo cp cppan /usr/local/bin/
```

### Build

```
git clone https://github.com/cppan/cppan cppan_client
cd cppan_client
cppan
mkdir build && cd build
cmake ..
# for linux
# cmake .. -DCMAKE_C_COMPILER=gcc-5 -DCMAKE_CXX_COMPILER=g++-5
make -j4
```

### Support CPPAN

More info about supporting C++ Archive Network can be found [here](https://github.com/cppan/cppan/blob/master/doc/support.md).
