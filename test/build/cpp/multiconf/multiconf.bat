sw ^
    %* ^
    -sfc ^
    -configuration Debug,ReleaseWithDebInfo,MinSizeRel,Release ^
    -platform win32,win64,arm,arm64 ^
    -compiler msvc ^
    -static -shared ^
    -mt -md ^
    build

:: clangcl does not work for arm

sw ^
    %* ^
    -sfc ^
    -configuration Debug,ReleaseWithDebInfo,MinSizeRel,Release ^
    -platform win32,win64 ^
    -compiler clangcl ^
    -static -shared ^
    -mt -md ^
    build

sw ^
    %* ^
    -sfc ^
    -configuration Debug,ReleaseWithDebInfo,MinSizeRel,Release ^
    -platform arm64 ^
    -compiler clangcl ^
    -static -shared ^
    -mt -md ^
    build

:: clang does not work for x86,arm,aarch64

sw ^
    %* ^
    -sfc ^
    -configuration Debug,ReleaseWithDebInfo,MinSizeRel,Release ^
    -platform win64 ^
    -compiler clang ^
    -static -shared ^
    -mt -md ^
    build
