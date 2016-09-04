# THIS FILE IS OUTDATED! Use this document https://github.com/cppan/cppan/blob/master/doc/cppan.yml 

This page describes `cppan.yml` command file and its directives. See working examples in this repository https://github.com/cppan/packages.

### files

In `files` directive you specify what files to include into the distributable package. It can be a regex expression or a relative file name.

    files:
        - include/.*
        - src/.*
or

    files:
        - sqlite3.h
        - sqlite3ext.h
        - sqlite3.c

or just

    files: include/.*

or 
  
    files: # from google.protobuf
      - src/.*\.h
      - src/google/protobuf/arena.cc
      - src/google/protobuf/arenastring.cc
      - src/google/protobuf/extension_set.cc 

### dependencies

`dependencies` contains a list of all dependencies required by your project. Can be `private` and `public`. `public` are exported when you add you project to C++ Archive Network. `private` stays private and can be used by your project's tools and other stuff.

For example when you develop an application, you want to add unit tests, regression tests, benchmarks etc. You can specify test frameworks as dependencies too. For example, `pvt.cppan.demo.google.googletest.gtest` or `pvt.cppan.demo.google.googletest.gmock`.

But when you develop a library and want export it to CPPAN, you won't those libraries in the public dependency list.
You can write:

    dependencies:
      public:
        org.boost.filesystem: 1
      private:
        pvt.cppan.demo.google.googletest.gtest: master

By default all deps are public.

    dependencies:
        org.boost.filesystem: 1.60 # public
        pvt.cppan.demo.google.googletest.gtest: master # public now

### include_directories

Include directories are needed by your project and project users. You must always write `private` or `public` keywords. `private` include dirs available for your lib only. `public` available for your lib and its users.

    include_directories: # boost.math example
      public:
        - include
      private:
        - src/tr1

### license

Include license file if you have it in the project tree.

    license: LICENSE.txt

### root_directory

When adding version from remote file (archive) often there is a root dir inside the archive. You can use this directive to specify a path to be added to all relative files and dirs.

    root_directory: sqlite-amalgamation-3110000 # sqlite3 example

### exclude_from_build

Sometimes you want to ship a source file, but do not want to include it into build. Maybe it will be conditionally included from config header or whatever.

    exclude_from_build: # from boost.thread
      - src/pthread/once_atomic.cpp

### options

In `options` you can provide predefined macros for specific configurations.
`any`, `static` and `shared` are available.
Inside them you can use `definitions` to provide compile defines. You must write `public` or `private` near each define. `private` is only for current library. `public` define will see all users and the current lib.

    options: # from boost.log
        any:
          definitions:
            public: BOOST_LOG_WITHOUT_EVENT_LOG
            private: BOOST_LOG_BUILDING_THE_LIB=1
        shared:
          definitions:
            public: BOOST_LOG_DYN_LINK
            private: BOOST_LOG_DLL
    
But try to minimize use of such options.

### pre_sources, post_sources, post_target, post_alias

You can provide your custom build system insertions with these directives.

    post_sources: | # custom step from boost.config
      if (WIN32)
        file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/include/boost/config/auto_link.hpp "")
      endif() 

Can be used in options too near with `definitions`.

### root_project, projects

Can be used when you're exporting more than one project from one repository. Inside `projects` you should describe all exported projects. Relative name of each should be specified after this. Inside each project, write usual command.

    root_project: pvt.cppan.demo.google.googletest
      projects:
        gtest: # use relative name here - absolute will be pvt.cppan.demo.google.googletest.gtest
          license: googletest/LICENSE
          files:
            - googletest/include/.*\.h
            - googletest/src/.*
        gmock: # use relative name here
          # ...

### package_dir

Can be used to choose storage of downloaded packages. Could be `system`, `user`, `local`. Default is `user`. `system` requires root right when running cppan, deps will go to `/usr/share/cppan/packages`. `user` stores deps in `$HOME/.cppan/packages`. `local` will store deps in cppan/ in local dir.

    package_dir: local

You can selectively choose what deps should go to one of those locations.

    dependencies:
        org.boost.filesystem:
            version: "*"
            package_dir: local

It is convenient when you want to apply your patches to some dependency.

###  check_function_exists, check_include_exists, check_type_size, check_symbol_exists, check_library_exists

These use cmake facilities to provide a way of checking headers, symbols, functions etc. Cannot be under `projects` directive. Can be only under root. They will be gathered during `cppan` run, duplicates will be removed, so not duplicate work.

    check_function_exists: # from libressl
      - asprintf
      - inet_pton
      - reallocarray

    check_include_exists:
      - err.h
