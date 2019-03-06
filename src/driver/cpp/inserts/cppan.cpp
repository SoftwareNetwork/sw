// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This is precompiled header for config files.
// Also it contain some control routines for their work.

#include <string>

#ifdef CPPAN_OS_WINDOWS
#include <Windows.h>
#include <Delayimp.h>
#include <comdef.h>

static HMODULE GetCurrentModule()
{
    HMODULE hModule = NULL;
    // hModule is NULL if GetModuleHandleEx fails.
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCTSTR)GetCurrentModule, &hModule);
    return hModule;
}

FARPROC WINAPI delayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
    // fix library name to current executable
    if (dliNotify == dliNotePreLoadLibrary && strcmp(pdli->szDll, IMPORT_LIBRARY) == 0)
    {
        return (FARPROC)GetModuleHandle(0);
        return (FARPROC)GetCurrentModule();
    }
    return NULL;
}

const PfnDliHook __pfnDliNotifyHook2 = delayLoadHook;

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        // For optimization.
        //DisableThreadLibraryCalls(h);

        // load all imports on startup
        auto hr = __HrLoadAllImportsForDll(IMPORT_LIBRARY);
        if (FAILED(hr))
        {
            _com_error err(hr);
            auto errMsg = err.ErrorMessage();

            printf("Failed on snap load of " IMPORT_LIBRARY ", exiting: %ws\n", errMsg);
            return FALSE;
        }
    }
    return TRUE;
}
#endif
