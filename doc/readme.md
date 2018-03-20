# [C++ Archive Network (CPPAN)](https://cppan.org/) Documentation

## Quick Start

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

Report if anything goes wrong with this workflow!

## More links

- [Getting Started](https://github.com/cppan/cppan/blob/master/doc/getting_started.md) - Running CPPAN client.
- [Config Options](https://github.com/cppan/cppan/blob/master/doc/cppan.yml) - Tuning your dependencies, prepare your project to be added to CPPAN, available config options.
- [Website Usage](https://github.com/cppan/cppan/blob/master/doc/website.md) - Describes basic usage of CPPAN site.
- [Build Definitions](https://github.com/cppan/cppan/blob/master/doc/cpp_definitions.md) - Preprocessor variables you can rely on during CPPAN build.
- [F.A.Q.](https://github.com/cppan/cppan/blob/master/doc/faq.md)
- [Support CPPAN](https://github.com/cppan/cppan/blob/master/doc/support.md)


