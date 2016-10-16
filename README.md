## [C++ Archive Network](https://cppan.org/)

### Resources

- Homepage: https://cppan.org/
- Docs: https://github.com/cppan/cppan/blob/master/doc/
- Download: https://cppan.org/client/
- Issue tracking: https://github.com/cppan/cppan/issues
- Forum: https://groups.google.com/forum/#!forum/cppan

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

[Download client](https://cppan.org/client/cppan-master-win32-client.zip), unzip it and put under PATH.

#### macOS

```
wget https://cppan.org/client/cppan-master-macOS-client.zip 
unzip cppan-master-macOS-client.zip 
sudo cp cppan /usr/local/bin/
```

### Build

```
git clone https://github.com/cppan/cppan
cd cppan
git submodule update --init
mkdir build && cd build
cmake ..
# for linux
# cmake .. -DCMAKE_C_COMPILER=gcc-5 -DCMAKE_CXX_COMPILER=g++-5
make -j4
```

On Windows you should download the latest LibreSSL and put it into 'dep/libressl' directory before cmake step.
