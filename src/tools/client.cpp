/*
 * Copyright (C) 2018 Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <Windows.h>
#include <boost/dll.hpp>

#define MESSAGE(m) MessageBox(NULL, m, L"Exe trampoline (sw.com):", MB_OK)

int wmain(int argc, wchar_t *argv[])
{
    const auto loc = boost::dll::program_location();
    auto p = loc.parent_path() / loc.filename().stem();
    p += L".exe";

    const auto prog = p.wstring();

    std::wstring cmd = GetCommandLine();
    int o = cmd[0] == '\"' ? 1 : 0;
    cmd.replace(cmd.find(argv[0]) - o, wcslen(argv[0]) + o + o, L"\"" + prog + L"\"");

    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcess(prog.c_str(), (LPWSTR)cmd.c_str(), 0, 0, 0, 0, 0, 0, &si, &pi))
    {
        auto e = GetLastError();
        WCHAR lpszBuffer[8192] = { 0 };
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, e, 0, lpszBuffer, 8192, NULL);
        std::wstring s = L"CreateProcess() failed: ";
        s += lpszBuffer;
        MESSAGE(s.c_str());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD r;
    if (GetExitCodeProcess(pi.hProcess, &r))
        return r;
    return 1;
}
