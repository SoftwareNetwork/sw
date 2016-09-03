if (MSVC)
    set(CPPAN_EXPORT "__declspec(dllexport)")
    set(CPPAN_IMPORT "__declspec(dllimport)")
endif()

if (MINGW)
    set(CPPAN_EXPORT "__attribute__((__dllexport__))")
    set(CPPAN_IMPORT "__attribute__((__dllimport__))")
elseif(GNU)
    set(CPPAN_EXPORT "__attribute__((__visibility__(\"default\")))")
    set(CPPAN_IMPORT)
endif()

if (SUN) # TODO: check it in real environment
    set(CPPAN_EXPORT "__global")
    set(CPPAN_IMPORT "__global")
endif()
