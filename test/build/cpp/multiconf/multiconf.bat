sw ^
    %* ^
    -configuration Debug,ReleaseWithDebInfo,MinSizeRel,Release ^
    -platform win32,win64,arm,arm64 ^
    -compiler msvc ^
    -static -shared ^
    -mt -md ^
    build

:: -compiler msvc,clangcl
