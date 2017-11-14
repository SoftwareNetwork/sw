CPPAN uses [CMake](cmake.org) build system at the moment. Minimum required version is 3.2. Later other build systems can be added.

We'll describe initial setup and usage based on this demo project https://github.com/cppan/demo_project.

To start work with cppan install its client into the system. On all systems and work can be done without root privileges.
  1. Create your project's initial structure, `CMakeLists.txt` file etc.
  2. Create `cppan.yml` file in the project's root directory.
  3. Open it and write dependencies on which your project depends.

`cppan.yml` files use YAML syntax which is very simple. Example of this file can be:

    dependencies:
        pvt.cppan.demo.gsl: master
        pvt.cppan.demo.yaml_cpp: "*"

        pvt.cppan.demo.boost.asio: 1
        pvt.cppan.demo.boost.filesystem: 1.60
        pvt.cppan.demo.boost.log: 1.60.0

`dependencies` directive tells the CPPAN which projects are used by your project. `*` means any latest fixed (not a branch) version. Textual name is a branch name (`master`). Versions can be `1.2.8` - exact version, `1.2` means `1.2.*` any version in 1.2 series, `1` means `1.*.*` and version in 1 series. When the new version is available it will be downloaded and replace your current one only in case if you use series versions. If you use strict fixed version, it won't be updated, so no surprises.

Now you should run `cppan` client in project's root directory where `cppan.yml` is located. It will download necessary packages and do initial build system setup.

After this you should include `.cppan` subdirectory in your `CMakeLists.txt` file.

    add_subdirectory(.cppan)

You can do this after `project()` directive. In this case all dependencies won't be re-built after any compile flags. If you need your special compile flags to have influence on deps, write `add_subdirectory(.cppan)` after compiler setup.

For your target(s) add `cppan` to `target_link_libraries()`:

    target_link_libraries(your_target cppan)

CMake will try to link all deps into this target. To be more specific you can provide only necessary deps tothis target:

    target_link_libraries(your_target org.boost.algorithm-1.60.0) # or
    target_link_libraries(your_target org.boost.algorithm-1.60) # or
    target_link_libraries(your_target org.boost.algorithm-1) # or
    target_link_libraries(your_target org.boost.algorithm) # or

All these names are aliases to full name. So, when you have more that 1 version of library (what is really really bad!), you can specify correct version.
For custom build steps you may use executables by their shortest name.

Internally cppan generate a `CMakeLists.txt` file for dependency. It will use all files it found in the dependency dir.

    file(GLOB_RECURSE src "*")
