// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Replace fake dll dependency with current running program

#include <Windows.h>
#include <Delayimp.h>
#include <comdef.h>

FARPROC WINAPI delayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
    // fix library name to current executable
    if (dliNotify == dliNotePreLoadLibrary && strcmp(pdli->szDll, IMPORT_LIBRARY) == 0)
        return (FARPROC)GetModuleHandle(0);
    return NULL;
}

const PfnDliHook __pfnDliNotifyHook2 = delayLoadHook;

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
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
