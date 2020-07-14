// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602

#include <sw/manager/storage.h>

#include <primitives/command.h>

#include <boost/algorithm/string.hpp>

#include <vector>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "sw.cli.run.windows");

#include <windows.h>
#include <strsafe.h>
#include <Sddl.h>
#include <Userenv.h>
#include <AccCtrl.h>
#include <Aclapi.h>

#pragma comment(lib, "Userenv.lib")

using CreateAppF =
HRESULT
(WINAPI*)(
    _In_ PCWSTR pszAppContainerName,
    _In_ PCWSTR pszDisplayName,
    _In_ PCWSTR pszDescription,
    _In_reads_opt_(dwCapabilityCount) PSID_AND_ATTRIBUTES pCapabilities,
    _In_ DWORD dwCapabilityCount,
    _Outptr_ PSID* ppSidAppContainerSid);

using DeriveAppF =
HRESULT
(WINAPI*)(
    _In_ PCWSTR pszAppContainerName,
    _Outptr_ PSID *ppsidAppContainerSid);

using DeleteAppF =
HRESULT
(WINAPI*)(
    _In_ PCWSTR pszAppContainerName);

//List of allowed capabilities for the application
// https://docs.microsoft.com/en-us/windows/desktop/api/winnt/ne-winnt-well_known_sid_type
std::vector<WELL_KNOWN_SID_TYPE> app_capabilities
{
    //WinCapabilityPrivateNetworkClientServerSid,
};

static BOOL SetSecurityCapabilities(PSID container_sid, SECURITY_CAPABILITIES *capabilities, PDWORD num_capabilities)
{
    DWORD sid_size = SECURITY_MAX_SID_SIZE;
    DWORD num_capabilities_ = app_capabilities.size() * sizeof(WELL_KNOWN_SID_TYPE) / sizeof(DWORD);
    SID_AND_ATTRIBUTES *attributes;
    BOOL success = TRUE;

    attributes = (SID_AND_ATTRIBUTES *)malloc(sizeof(SID_AND_ATTRIBUTES) * num_capabilities_);

    ZeroMemory(capabilities, sizeof(SECURITY_CAPABILITIES));
    ZeroMemory(attributes, sizeof(SID_AND_ATTRIBUTES) * num_capabilities_);

    for (unsigned int i = 0; i < num_capabilities_; i++)
    {
        attributes[i].Sid = malloc(SECURITY_MAX_SID_SIZE);
        if (!CreateWellKnownSid(app_capabilities[i], NULL, attributes[i].Sid, &sid_size))
        {
            success = FALSE;
            break;
        }
        attributes[i].Attributes = SE_GROUP_ENABLED;
    }

    if (success == FALSE)
    {
        for (unsigned int i = 0; i < num_capabilities_; i++)
        {
            if (attributes[i].Sid)
                LocalFree(attributes[i].Sid);
        }

        free(attributes);
        attributes = NULL;
        num_capabilities_ = 0;
    }

    capabilities->Capabilities = attributes;
    capabilities->CapabilityCount = num_capabilities_;
    capabilities->AppContainerSid = container_sid;
    *num_capabilities = num_capabilities_;

    return success;
}

static BOOL GrantNamedObjectAccess(PSID appcontainer_sid, const path &object_name, SE_OBJECT_TYPE object_type, DWORD access_mask)
{
    EXPLICIT_ACCESS explicit_access;
    PACL original_acl = NULL, new_acl = NULL;
    DWORD status;
    BOOL success = FALSE;

    do
    {
        explicit_access.grfAccessMode = GRANT_ACCESS;
        explicit_access.grfAccessPermissions = access_mask;
        explicit_access.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;

        explicit_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
        explicit_access.Trustee.pMultipleTrustee = NULL;
        explicit_access.Trustee.ptstrName = (TCHAR *)appcontainer_sid;
        explicit_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        explicit_access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;

        status = GetNamedSecurityInfo(object_name.wstring().c_str(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, &original_acl,
            NULL, NULL);
        if (status != ERROR_SUCCESS)
        {
            printf("GetNamedSecurityInfo() failed for %s, error: %d\n", object_name.u8string().c_str(), status);
            break;
        }

        status = SetEntriesInAcl(1, &explicit_access, original_acl, &new_acl);
        if (status != ERROR_SUCCESS)
        {
            printf("SetEntriesInAcl() failed for %s, error: %d\n", object_name.u8string().c_str(), status);
            break;
        }

        status = SetNamedSecurityInfo((LPTSTR)object_name.wstring().c_str(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, new_acl, NULL);
        if (status != ERROR_SUCCESS)
        {
            printf("SetNamedSecurityInfo() failed for %s, error: %d\n", object_name.u8string().c_str(), status);
            break;
        }

        success = TRUE;
    } while (0);

    // MSDN: no need to free original_acl
    //if (original_acl)
    //LocalFree(original_acl);

    if (new_acl)
        LocalFree(new_acl);

    return success;
}

void run1(const sw::LocalPackage &pkg, primitives::Command &c, bool gRunAppInContainer)
{
    PSID sid = NULL;
    SECURITY_CAPABILITIES SecurityCapabilities = { 0 };
    DWORD num_capabilities = 0;
    SIZE_T attribute_size = 0;
    BOOL success = FALSE;
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList = 0;

    String rel_name;
    if (pkg.getPath().isRelative())
        rel_name = "." + std::to_string(abs((long long)std::hash<path>()(c.getProgram())));

    auto container_name_s = to_wstring("sw.app." + pkg.getHash().substr(0, 32) + rel_name);
    auto container_name = container_name_s.c_str();

    auto pkg_s = to_wstring(pkg.toString());
    auto container_desc_s = pkg_s;
    if (pkg_s.size() > 512)
        pkg_s = container_name_s;
    if (container_desc_s.size() > 2048)
        container_desc_s = container_name_s;
    auto pkg_name = pkg_s.c_str();
    auto container_desc = container_desc_s.c_str();

    String err(1024, 0);

    // fix win7 startup
    auto userenv = LoadLibrary(L"Userenv.dll");
    if (!userenv)
        throw SW_RUNTIME_ERROR("Cannot load Userenv.dll");
    auto create_app = (CreateAppF)GetProcAddress(userenv, "CreateAppContainerProfile");
    auto derive_app = (DeriveAppF)GetProcAddress(userenv, "DeriveAppContainerSidFromAppContainerName");
    auto delete_app = (DeleteAppF)GetProcAddress(userenv, "DeleteAppContainerProfile");
    if (gRunAppInContainer && !(create_app && derive_app && delete_app))
        throw SW_RUNTIME_ERROR("Cannot launch app in container (no Windows APIs loaded)");

    do
    {
        if (gRunAppInContainer)
        {
            auto result = create_app(container_name, pkg_name, container_desc, NULL, 0, &sid);
            if (!SUCCEEDED(result))
            {
                if (HRESULT_CODE(result) == ERROR_ALREADY_EXISTS)
                {
                    result = derive_app(container_name, &sid);
                    if (!SUCCEEDED(result))
                    {
                        snprintf(err.data(), err.size(), "Failed to get existing AppContainer name, error code: %d", HRESULT_CODE(result));
                        break;
                    }
                }
                else
                {
                    snprintf(err.data(), err.size(), "Failed to create AppContainer, last error: %d\n", HRESULT_CODE(result));
                    break;
                }
            }

            if (!SetSecurityCapabilities(sid, &SecurityCapabilities, &num_capabilities))
            {
                snprintf(err.data(), err.size(), "Failed to set security capabilities, last error: %d\n", GetLastError());
                break;
            }

            // set permissions
            auto grant_perms = [&sid, &err](const Files &paths, DWORD mode)
            {
                if (!std::all_of(paths.begin(), paths.end(), [&sid, &err, mode](auto &p)
                {
                    if (!GrantNamedObjectAccess(sid, p, SE_FILE_OBJECT, mode))
                    {
                        snprintf(err.data(), err.size(), "Failed to grant explicit access to '%s'\n", p.u8string().c_str());
                        return false;
                    }
                    return true;
                }))
                {
                    return false;
                }
                return true;
            };

            //
            Files paths;
            if (!c.working_directory.empty())
                paths.insert(c.working_directory);
            paths.insert(pkg.getDirSrc2());
            paths.insert(normalize_path_windows(path(c.getProgram()).parent_path()));

            if (!grant_perms(paths, FILE_ALL_ACCESS & ~DELETE))
                break;

            if (c.environment.find("PATH") != c.environment.end())
            {
                auto dirs = split_string(c.environment["PATH"], ";");
                Files paths;
                for (auto &d : dirs)
                {
                    // we cannot set rights on c:\\windows
                    if (boost::to_upper_copy(normalize_path_windows(d)).find("C:\\WINDOWS") == 0)
                        continue;
                    paths.insert(d);
                }
                if (!grant_perms(paths, FILE_GENERIC_READ))
                    break;
            }

            int n = 1 + 1; // +1 for uv std handles
            InitializeProcThreadAttributeList(NULL, n, NULL, &attribute_size);
            lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attribute_size);

            if (!InitializeProcThreadAttributeList(lpAttributeList, n, NULL, &attribute_size))
            {
                snprintf(err.data(), err.size(), "InitializeProcThreadAttributeList() failed, last error: %d", GetLastError());
                break;
            }

            if (!UpdateProcThreadAttribute(lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
                &SecurityCapabilities, sizeof(SecurityCapabilities), NULL, NULL))
            {
                snprintf(err.data(), err.size(), "UpdateProcThreadAttribute() failed, last error: %d", GetLastError());
                break;
            }

            c.attribute_list = lpAttributeList;
            // this allows us to see error message
            c.detached = false;

            // if relative app, we must delete container later
            if (pkg.getPath().isRelative())
                c.detached = false;
        }

        error_code ec;
        c.execute(ec);

        if (gRunAppInContainer && pkg.getPath().isRelative())
        {
            auto result = delete_app(container_name);
            if (!SUCCEEDED(result))
            {
                LOG_WARN(logger, "Cannot remove app container");
            }
        }

        success = !ec;
    } while (0);

    if (lpAttributeList)
    {
        DeleteProcThreadAttributeList(lpAttributeList);
        free(lpAttributeList);
    }

    if (SecurityCapabilities.Capabilities)
        free(SecurityCapabilities.Capabilities);

    if (sid)
        FreeSid(sid);

    if (!success)
    {
        if (std::any_of(err.begin(), err.end(), [](auto e) {return e != 0;}))
            throw SW_RUNTIME_ERROR(err.c_str()); // to strip nulls
        throw SW_RUNTIME_ERROR(c.getError());
    }
}

#endif
