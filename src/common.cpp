/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include "stamp.h"

#include <codecvt>
#include <fstream>
#include <locale>
#include <random>
#include <regex>

#include <curl/curl.h>
#include <curl/easy.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#ifdef WIN32
#include <windows.h>

#include <Winhttp.h>
#pragma comment (lib, "Winhttp.lib")

#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>
#else
#include <archive.h>
#include <archive_entry.h>
#endif

#ifdef __APPLE__
#include <libproc.h>
#include <unistd.h>
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <linux/limits.h>
#endif

static const std::regex r_project_version_number(R"((\d+).(\d+).(\d+))");
static const std::regex r_branch_name(R"(([a-zA-Z_][a-zA-Z0-9_-]*))");
static const std::regex r_version1(R"((\d+))");
static const std::regex r_version2(R"((\d+).(\d+))");
static const std::regex r_version3(R"((-?\d+).(-?\d+).(-?\d+))");

HttpSettings httpSettings;

Version get_program_version()
{
    return{ VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
}

String get_program_version_string(const String &prog_name)
{
    boost::posix_time::ptime t(boost::gregorian::date(1970, 1, 1));
    t += boost::posix_time::seconds(static_cast<long>(std::stoi(cppan_stamp)));
    return prog_name + " version " + get_program_version().toString() + "\n" +
        "assembled " + boost::posix_time::to_simple_string(t);
}

Version::Version(ProjectVersionNumber ma, ProjectVersionNumber mi, ProjectVersionNumber pa)
    : major(ma), minor(mi), patch(pa)
{

}

Version::Version(const String &s)
{
    if (s == "*")
        return;
    std::smatch m;
    if (std::regex_match(s, m, r_version3))
    {
        major = std::stoi(m[1].str());
        minor = std::stoi(m[2].str());
        patch = std::stoi(m[3].str());
    }
    else if (std::regex_match(s, m, r_version2))
    {
        major = std::stoi(m[1].str());
        minor = std::stoi(m[2].str());
    }
    else if (std::regex_match(s, m, r_version1))
    {
        major = std::stoi(m[1].str());
    }
    else if (std::regex_match(s, m, r_branch_name))
    {
        branch = m[1].str();
        String error;
        if (!check_branch_name(branch, &error))
            throw std::runtime_error(error);
    }
    else
        throw std::runtime_error("Bad version");
    if (!isValid())
        throw std::runtime_error("Bad version");
}

String Version::toString() const
{
    if (!branch.empty())
        return branch;
    String s;
    s += std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    return s;
}

String Version::toAnyVersion() const
{
    if (!branch.empty())
        return branch;
    if (major == -1 && minor == -1 && patch == -1)
        return "*";
    String s;
    s += std::to_string(major) + ".";
    if (minor != -1)
        s += std::to_string(minor) + ".";
    if (patch != -1)
        s += std::to_string(patch) + ".";
    s.resize(s.size() - 1);
    return s;
}

path Version::toPath() const
{
    if (!branch.empty())
        return branch;
    path p;
    p /= std::to_string(major);
    p /= std::to_string(minor);
    p /= std::to_string(patch);
    return p;
}

bool Version::isValid() const
{
    if (!branch.empty())
        return check_branch_name(branch);
    if (major == 0 && minor == 0 && patch == 0)
        return false;
    if (major < -1 || minor < -1 || patch < -1)
        return false;
    return true;
}

bool Version::operator<(const Version &rhs) const
{
    if (isBranch() && rhs.isBranch())
        return branch < rhs.branch;
    if (isBranch())
        return true;
    if (rhs.isBranch())
        return false;
    return std::tie(major, minor, patch) < std::tie(rhs.major, rhs.minor, rhs.patch);
}

bool Version::operator==(const Version &rhs) const
{
    if (isBranch() && rhs.isBranch())
        return branch == rhs.branch;
    if (isBranch() || rhs.isBranch())
        return false;
    return std::tie(major, minor, patch) == std::tie(rhs.major, rhs.minor, rhs.patch);
}

bool Version::operator!=(const Version &rhs) const
{
    return !operator==(rhs);
}

bool Version::canBe(const Version &rhs) const
{
    if (*this == rhs)
        return true;

    // *.*.* canBe anything
    if (major == -1 && minor == -1 && patch == -1)
        return true;

    // 1.*.* == 1.*.*
    if (major == rhs.major && minor == -1 && patch == -1)
        return true;

    // 1.2.* == 1.2.*
    if (major == rhs.major && minor == rhs.minor && patch == -1)
        return true;

    return false;
}

bool check_branch_name(const String &n, String *error)
{
    if (!std::regex_match(n, r_branch_name))
    {
        if (error)
            *error = "Branch name should be a-zA-Z0-9_- starting with letter or _";
        return false;
    }
    return true;
}

Files unpack_file(const path &fn, const path &dst)
{
    if (!fs::exists(dst))
        fs::create_directories(dst);

    Files files;

    auto a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    auto r = archive_read_open_filename(a, fn.string().c_str(), 10240);
    if (r != ARCHIVE_OK)
        throw std::runtime_error(archive_error_string(a));
    archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
    {
        path f = dst / archive_entry_pathname(entry);
        path fdir = f.parent_path();
        if (!fs::exists(fdir))
            fs::create_directories(fdir);
        path filename = f.filename();
        if (filename == "." || filename == "..")
            continue;
        auto fn = fs::absolute(f).string();
        std::ofstream o(fn, std::ios::out | std::ios::binary);
        if (!o)
        {
            // TODO: probably remove this and linux/limit.h header when server will be using hash paths
#ifdef _WIN32
            if (fn.size() >= MAX_PATH)
                continue;
#elif defined(__APPLE__)
#else
            if (fn.size() >= PATH_MAX)
                continue;
#endif
            throw std::runtime_error("Cannot open file: " + f.string());
        }
        for (;;)
        {
            const void *buff;
            size_t size;
            int64_t offset;
            auto r = archive_read_data_block(a, &buff, &size, &offset);
            if (r == ARCHIVE_EOF)
                break;
            if (r < ARCHIVE_OK)
                throw std::runtime_error(archive_error_string(a));
            o.write((const char *)buff, size);
        }
        files.insert(f);
    }
    archive_read_close(a);
    archive_read_free(a);

    return files;
}

DownloadData::DownloadData()
{
}

DownloadData::~DownloadData()
{
    if (ctx)
    {
        EVP_MD_CTX_cleanup(ctx.get());
    }
}

void DownloadData::finalize()
{
    if (!ctx)
        return;
    uint32_t hash_size = 0;
    uint8_t hash[EVP_MAX_MD_SIZE] = { 0 };
    EVP_DigestFinal_ex(ctx.get(), hash, &hash_size);
    if (dl_md5)
        *dl_md5 = hash_to_string(hash, hash_size);
}

size_t DownloadData::progress(char *ptr, size_t size, size_t nmemb)
{
    if (dl_md5)
    {
        if (!ctx)
        {
            ctx = std::make_unique<EVP_MD_CTX>();
            EVP_MD_CTX_init(ctx.get());
            EVP_MD_CTX_set_flags(ctx.get(), EVP_MD_CTX_FLAG_ONESHOT);
            EVP_DigestInit(ctx.get(), EVP_md5());
        }
    }
    auto read = size * nmemb;
    ofile->write(ptr, read);
    if (ctx)
        EVP_DigestUpdate(ctx.get(), ptr, read);
    return read;
}

size_t curl_write_file(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    DownloadData &data = *(DownloadData *)userdata;
    return data.progress(ptr, size, nmemb);
}

int curl_transfer_info(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    int64_t file_size_limit = *(int64_t*)clientp;
    if (dlnow > file_size_limit)
        return 1;
    return 0;
}

size_t curl_write_string(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    String &s = *(String *)userdata;
    auto read = size * nmemb;
    s.append(ptr, ptr + read);
    return read;
}

String url_post(const String &url, const String &data)
{
    auto curl = curl_easy_init();

#ifdef _WIN32
    // FIXME: remove after new curl released (> 7.49.1)
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif

    if (httpSettings.verbose)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    // proxy settings
    auto proxy_addr = getAutoProxy();
    if (!proxy_addr.empty())
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_addr.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    }
    if (!httpSettings.proxy.host.empty())
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, httpSettings.proxy.host.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
        if (!httpSettings.proxy.user.empty())
            curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, httpSettings.proxy.user.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_string);
    if (url.find("https") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
        if (httpSettings.ignore_ssl_checks)
        {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
            //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0);
        }
    }
    String response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    auto res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK)
        throw std::runtime_error(String(curl_easy_strerror(res)));
    return response;
}

ptree url_post(const String &url, const ptree &data)
{
    ptree p;
    std::ostringstream oss;
    pt::write_json(oss, data
#if !defined(CPPAN_TEST)
        , false
#endif
    );
    auto response = url_post(url, oss.str());
    std::istringstream iss(response);
    pt::read_json(iss, p);
    return p;
}

void download_file(DownloadData &data)
{
    auto parent = data.fn.parent_path();
    if (!parent.empty() && !fs::exists(parent))
        fs::create_directories(parent);
    std::ofstream ofile(data.fn.string(), std::ios::out | std::ios::binary);
    if (!ofile)
        throw std::runtime_error("Cannot open file: " + data.fn.string());
    data.ofile = &ofile;

    // set up curl request
    auto curl = curl_easy_init();

#ifdef _WIN32
    // FIXME: remove after new curl released (> 7.49.1)
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0);
#endif

    if (httpSettings.verbose)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    curl_easy_setopt(curl, CURLOPT_URL, data.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    // proxy settings
    auto proxy_addr = getAutoProxy();
    if (!proxy_addr.empty())
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_addr.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    }
    if (!httpSettings.proxy.host.empty())
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, httpSettings.proxy.host.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
        if (!httpSettings.proxy.user.empty())
            curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, httpSettings.proxy.user.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_transfer_info);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &data.file_size_limit);
    if (data.url.find("https") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
        if (httpSettings.ignore_ssl_checks)
        {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
            //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0);
        }
    }
    auto res = curl_easy_perform(curl);
    data.finalize();
    curl_easy_cleanup(curl);
    if (res == CURLE_ABORTED_BY_CALLBACK)
    {
        fs::remove(data.fn);
        throw std::runtime_error("File '" + data.url + "' is too big. Limit is " + std::to_string(data.file_size_limit) + " bytes.");
    }
    if (res != CURLE_OK)
        throw std::runtime_error(String(curl_easy_strerror(res)));
}

String generate_random_sequence(uint32_t len)
{
    auto seed = std::random_device()();
    std::mt19937 g(seed);
    std::uniform_int_distribution<> d(0, 127);
    String user_session(len, 0);
    while (len)
    {
        char c;
        do c = (char)d(g);
        while (!isalnum(c));
        user_session[--len] = c;
    }
    return user_session;
};

String hash_to_string(const uint8_t *hash, uint32_t hash_size)
{
    String s;
    constexpr auto alnum16 = "0123456789abcdef";
    for (uint32_t i = 0; i < hash_size; i++)
    {
        s += alnum16[(hash[i] & 0xF0) >> 4];
        s += alnum16[(hash[i] & 0x0F) >> 0];
    }
    return s;
}

String sha1(const String &data)
{
    uint8_t hash[EVP_MAX_MD_SIZE];
    uint32_t hash_size;
    EVP_Digest(data.data(), data.size(), hash, &hash_size, EVP_sha1(), nullptr);
    return hash_to_string(hash, hash_size);
}

bool check_filename(const String &s, String *error)
{
    for (auto &c : s)
    {
        if (isalnum((uint8_t)c))
            continue;
        switch (c)
        {
        case '/':
        case '\\':
        case ':':
        case '.':
        case '_':
        case '-':
        case '+':
            break;
        default:
            return false;
        }
    }
    return true;
}

inline auto& get_string_converter()
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter;
}

std::wstring to_wstring(const std::string &s)
{
    auto &converter = get_string_converter();
    return converter.from_bytes(s.c_str());
}

std::string to_string(const std::wstring &s)
{
    auto &converter = get_string_converter();
    return converter.to_bytes(s.c_str());
}

String getAutoProxy()
{
    String proxy_addr;
    std::wstring wproxy_addr;
#ifdef _WIN32
    WINHTTP_PROXY_INFO proxy = { 0 };
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxy2 = { 0 };
    if (WinHttpGetDefaultProxyConfiguration(&proxy) && proxy.lpszProxy)
        wproxy_addr = proxy.lpszProxy;
    else if (WinHttpGetIEProxyConfigForCurrentUser(&proxy2) && proxy2.lpszProxy)
        wproxy_addr = proxy2.lpszProxy;
    proxy_addr = to_string(wproxy_addr);
#endif
    return proxy_addr;
}

String repeat(const String &e, int n)
{
    String s;
    if (n < 0)
        return s;
    s.reserve(e.size() * n);
    for (int i = 0; i < n; i++)
        s += e;
    return s;
}

path get_program()
{
#ifdef _WIN32
    WCHAR fn[8192] = { 0 };
    GetModuleFileNameW(NULL, fn, sizeof(fn) * sizeof(WCHAR));
    return fn;
#elif __APPLE__
    auto pid = getpid();
    char dest[PROC_PIDPATHINFO_MAXSIZE] = { 0 };
    auto ret = proc_pidpath(pid, dest, sizeof(dest));
    if (ret <= 0)
        throw std::runtime_error("Cannot get program path");
    return dest;
#else
    char dest[PATH_MAX];
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1)
    {
        perror("readlink");
        throw std::runtime_error("Cannot get program path");
    }
    return dest;
#endif
}
