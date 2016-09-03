The idea comes from:

1. Comprehensive Perl Archive Network (CPAN), CRAN (R-language), CTAN (TeX).
2. Java packages and build systems (Maven).
3. C++ Modules proposals and presentations by Gabriel Dos Reis.

In the beginning project aimed on C++ project with Modules only. So, the project should evolve by their release. But during development, CPPAN shows great capabilities of handling current C++98/C++11/C++14 projects and even some C libraries.

General principles of CPPAN are listed below.

1. Source code only! You do not include your other stuff like tests, benchmarks, utilities etc. Only headers and sources (if any). On exception here: project's license file. Include it if you have one in the project tree.
1. [Semantic versioning](http://semver.org).
2. Zero-configure (zero-conf.) projects. Projects should contain their configurations in headers (relying on toolchain macros) or rely on CPPAN utilities (macros, different checkers in configuration file) or have no config steps at all (header only projects). Still CPPAN provides inlining of user configuration steps, compiler flags etc.
3. All or Nothing rule for dependencies. Many projects have optional dependencies. In CPPAN they should list them and they'll be always included to the build or not included at all. So, no optional dependencies.

Projects' naming

Project names are like Modules from this C++ proposal http://open-std.org/JTC1/SC22/WG21/docs/papers/2016/p0143r1.pdf. Words are delimeted by points `.`.

Root names are:

1. pvt - for users' projects. E.g. `pvt.cppan.some_lib`.
2. org - for organization projects with open source licenses. E.g. `org.boost.algorithm`.
3. com - for organization projects with commercial licenses. E.g. `com.some_org.some_proprietary_lib`.

Project versions

Each project will have its versions. Version can be a semver number `1.2.8` or a branch name `master`, `dev`. Branches always considered as unstable unless it is stated by maintainer. Branches can be updated to the latest commit. And fixed versions can not.
