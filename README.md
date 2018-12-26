# [Software Network (SW)](https://software-network.org/)

[![Build status](https://ci.appveyor.com/api/projects/status/3mf8eall4lf764sk/branch/master?svg=true)](https://ci.appveyor.com/project/egorpugin/sw/branch/master)

## Resources

- Homepage: https://software-network.org/
- Docs: https://github.com/SoftwareNetwork/sw/tree/master/doc/
- Download: https://github.com/SoftwareNetwork/binaries
- Issue tracking: https://github.com/SoftwareNetwork/sw/issues
- Forum: https://groups.google.com/forum/#!forum/software-network

## Build

### Using SW (self build)

1. Download client from https://github.com/SoftwareNetwork/binaries
2. Unpack, add to PATH
3. Run
```
git clone https://github.com/SoftwareNetwork/sw
cd sw
sw build
```

(optional) Run `sw generate` to generate VS solution.

### Using CPPAN

1. Download CPPAN client from https://cppan.org/client/
2. Run 
```
git clone https://github.com/SoftwareNetwork/sw
cd sw
cppan --generate .
```
3. Open generated solution file for Visual Studio.

For other build types or OSs, run cppan with config option `cppan --generate . --config gcc8_debug`.

Check out config options at https://github.com/SoftwareNetwork/sw/blob/master/cppan.yml#L7

### Support SW

More info about supporting Software Network can be found [here](https://github.com/SoftwareNetwork/sw/blob/master/doc/support.md).
