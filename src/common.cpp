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

#include <fstream>
#include <random>
#include <regex>

#include <curl/curl.h>
#include <curl/easy.h>

#ifdef WIN32
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>
#else
#include <archive.h>
#include <archive_entry.h>
#endif

static const std::regex r_login("[a-z][a-z0-9_]+");
static const std::regex r_org_name = r_login;
static const std::regex r_project_name("[a-z_][a-z0-9_]+");
static const std::regex r_project_version_number(R"((\d+).(\d+).(\d+))");
static const std::regex r_branch_name(R"(([a-zA-Z_][a-zA-Z0-9_-]*))");
static const std::regex r_version1(R"((\d+))");
static const std::regex r_version2(R"((\d+).(\d+))");
static const std::regex r_version3(R"((-?\d+).(-?\d+).(-?\d+))");

Version get_program_version()
{
    return{ VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
}

String get_program_version_string(const String &prog_name)
{
    return prog_name + " version " + get_program_version().toString();
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
        throw std::runtime_error("Bad version number");
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
    if (major < -1 && minor < -1 && patch < -1)
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

std::tuple<int, String> system_with_output(const String &cmd)
{
#ifdef WIN32
#define popen _popen
#define pclose _pclose
#endif
    String s;
    auto f = popen(cmd.c_str(), "r");
    if (!f)
        return std::make_tuple(-1, s);
    constexpr int sz = 128;
    char buf[sz];
    while (fgets(buf, sz, f))
        s += buf;
    int ret = pclose(f);
    ret /= 256;
    return std::make_tuple(ret, s);
}

std::tuple<int, String> system_with_output(const std::vector<String> &args)
{
    String cmd;
    for (auto &a : args)
        cmd += a + " ";
    return system_with_output(cmd);
}

bool check_login(const String &n, String *error)
{
    if (!std::regex_match(n, r_login))
    {
        if (error)
            *error = "Username should contain alphanumeric characters or undescore symbols "
            "starting with an alpha and minimum 2 characters length";
        return false;
    }
    return true;
}

bool check_org_name(const String &n, String *error)
{
    if (!std::regex_match(n, r_org_name))
    {
        if (error)
            *error = "Organization name should contain alphanumeric characters or undescore symbols starting with an alpha";
        return false;
    }
    return true;
}

bool check_project_name(const String &n, String *error)
{
    if (!std::regex_match(n, r_project_name))
    {
        if (error)
            *error = "Project name should be like a C++ identifier";
        return false;
    }
    return true;
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

void unpack_file(const path &fn, const path &dst)
{
    if (!fs::exists(dst))
        fs::create_directories(dst);

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
        {
            continue;
        }
        std::ofstream o(f.string(), std::ios::out | std::ios::binary);
        if (!o)
            throw std::runtime_error("Cannot open file: " + f.string());
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
    }
    archive_read_close(a);
    archive_read_free(a);
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

size_t write_file(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    DownloadData &data = *(DownloadData *)userdata;
    return data.progress(ptr, size, nmemb);
}

int transfer_info(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    int64_t file_size_limit = *(int64_t*)clientp;
    if (dlnow > file_size_limit)
        return 1;
    return 0;
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
    auto curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, data.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_info);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &data.file_size_limit);
    if (data.url.find("https") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
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

String read_file(const path &p)
{
    if (!fs::exists(p))
        throw std::runtime_error("File '" + p.string() + "' does not exist");

    auto fn = p.string();
    std::ifstream ifile(fn);
    if (!ifile)
        throw std::runtime_error("Cannot open file " + fn);

    auto sz = fs::file_size(p);
    if (sz > 1'000'000)
        throw std::runtime_error("File " + fn + " is very big (> ~1MB)");

    String f, s;
    f.reserve((int)sz + 1);
    while (std::getline(ifile, s))
        f += s + "\n";
    return f;
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

String make_archive_name(const String &fn)
{
    return fn + ".tar.gz";
}

String sha1(const String &data)
{
    uint8_t hash[EVP_MAX_MD_SIZE];
    uint32_t hash_size;
    EVP_Digest(data.data(), data.size(), hash, &hash_size, EVP_sha1(), nullptr);
    return hash_to_string(hash, hash_size);
}

path temp_directory_path()
{
    auto p = fs::temp_directory_path() / "cppan";
    fs::create_directory(p);
    return p;
}

path get_temp_filename()
{
    return temp_directory_path() / fs::unique_path();
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
