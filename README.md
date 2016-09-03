# [C++ Archive Network](https://cppan.org/)

- [Website](https://cppan.org/)
- Read [Documentation](https://github.com/cppan/cppan/blob/master/doc/readme.md) to get help with CPPAN.
- [Download](https://cppan.org/client/) precompiled client binaries.
- Use [Issues](https://github.com/cppan/cppan/issues) page to put your feedback, bugs and other stuff.

## Download & Install

### Linux

#### Ubuntu 16.04

```
wget https://cppan.org/client/cppan-0.1.8-Linux-client.deb 
sudo dpkg -i cppan-0.1.8-Linux-client.deb
```

#### Ubuntu 14.04

```
sudo add-apt-repository ppa:ubuntu-toolchain-r/test 
sudo apt-get update && sudo apt-get install gcc-5 
wget https://cppan.org/client/cppan-0.1.8-Linux-client.deb 
sudo dpkg -i cppan-0.1.8-Linux-client.deb
```

### Windows

Download client, unzip it and put under PATH.

### macOS

```
wget https://cppan.org/client/cppan-master-macOS-client.zip 
unzip cppan-master-macOS-client.zip 
sudo cp cppan /usr/local/bin 
```

## Build

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
