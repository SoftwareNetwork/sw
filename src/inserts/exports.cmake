set(CPPAN_EXPORT)
set(CPPAN_IMPORT)

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
    # if (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x550) \
    #   || (defined(__SUNPRO_C) && __SUNPRO_C >= 0x550)
    set(CPPAN_EXPORT "__global")
    set(CPPAN_IMPORT "__global")
endif()

set(CPPAN_EXPORT ${CPPAN_EXPORT} PARENT_SCOPE)
set(CPPAN_IMPORT ${CPPAN_IMPORT} PARENT_SCOPE)
