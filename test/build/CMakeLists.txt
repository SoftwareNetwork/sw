cmake_minimum_required(VERSION 3.0)

project(basic)

file(GLOB_RECURSE src "build/c/exe/*")
add_executable(exe_c ${src})

file(GLOB_RECURSE src "build/cpp/exe/*")
add_executable(exe ${src})
target_compile_definitions(exe PUBLIC
    AND_MY_STRING=""
)

file(GLOB_RECURSE src "build/cpp/dll/*")
add_library(mydll SHARED ${src})

file(GLOB_RECURSE src "build/cpp/lib/*")
add_library(mylib ${src})




project(deps)

file(GLOB_RECURSE src "build/cpp/dep/exe_dll/main.cpp")
add_executable(exe1 ${src})
target_link_libraries(exe1 dll1)

file(GLOB_RECURSE src "build/cpp/dep/exe_dll/a*")
add_library(dll1 SHARED ${src})
set(def "MY_API=__declspec(dllexport)")
set(def2 "MY_API=__declspec(dllimport)")
target_compile_definitions(dll1
    PRIVATE ${def}
    INTERFACE ${def2}
)

file(GLOB_RECURSE src "build/cpp/dep/exe_lib/main.cpp")
add_executable(exe2 ${src})
target_link_libraries(exe2 lib2)

file(GLOB_RECURSE src "build/cpp/dep/exe_lib/a*")
add_library(lib2 ${src})
set(def "MY_API=")
target_compile_definitions(lib2
    PUBLIC ${def}
)

file(GLOB_RECURSE src "build/cpp/dep/exe_dll_dll/main.cpp")
add_executable(exe3 ${src})
target_link_libraries(exe3 dll3a dll3b)

file(GLOB_RECURSE src "build/cpp/dep/exe_dll_dll/a*")
add_library(dll3a SHARED ${src})
set(def "A_API=__declspec(dllexport)")
set(def2 "A_API=__declspec(dllimport)")
target_compile_definitions(dll3a
    PRIVATE ${def}
    INTERFACE ${def2}
)

file(GLOB_RECURSE src "build/cpp/dep/exe_dll_dll/b*")
add_library(dll3b SHARED ${src})
set(def "B_API=__declspec(dllexport)")
set(def2 "B_API=__declspec(dllimport)")
target_compile_definitions(dll3b
    PRIVATE ${def}
    INTERFACE ${def2}
)


