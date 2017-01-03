/*
 * Copyright (C) 2016-2017, Egor Pugin
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

#include "http.h"

#include "hash.h"
#include "stamp.h"
#include "version.h"

#include <codecvt>
#include <fstream>
#include <locale>
#include <random>
#include <regex>

#include <curl/curl.h>
#include <curl/easy.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#ifdef WIN32
#include <windows.h>

#include <Winhttp.h>
#pragma comment (lib, "Winhttp.lib")
#endif

HttpSettings httpSettings;

auto& get_string_converter()
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

void DownloadData::Hasher::finalize()
{
    if (!ctx)
        return;
    uint32_t hash_size = 0;
    uint8_t h[EVP_MAX_MD_SIZE] = { 0 };
#ifndef CPPAN_BUILD
    EVP_DigestFinal_ex(ctx.get(), h, &hash_size);
#else
    EVP_DigestFinal_ex(ctx, h, &hash_size);
#endif
    if (hash)
        *hash = hash_to_string(h, hash_size);
#ifndef CPPAN_BUILD
    ctx.reset();
#else
    EVP_MD_CTX_reset(ctx);
#endif
}

void DownloadData::Hasher::progress(char *ptr, size_t size, size_t nmemb)
{
    if (!hash)
        return;
    if (!ctx)
    {
#ifndef CPPAN_BUILD
        ctx = std::make_unique<EVP_MD_CTX>();
        EVP_MD_CTX_init(ctx.get());
        EVP_MD_CTX_set_flags(ctx.get(), EVP_MD_CTX_FLAG_ONESHOT);
        EVP_DigestInit(ctx.get(), hash_function());
#else
        ctx = EVP_MD_CTX_create();
        EVP_MD_CTX_init(ctx);
        EVP_MD_CTX_set_flags(ctx, EVP_MD_CTX_FLAG_ONESHOT);
        EVP_DigestInit(ctx, hash_function());
#endif
    }
#ifndef CPPAN_BUILD
    EVP_DigestUpdate(ctx.get(), ptr, size * nmemb);
#else
    EVP_DigestUpdate(ctx, ptr, size * nmemb);
#endif
}

DownloadData::Hasher::~Hasher()
{
    if (!ctx)
        return;
#ifndef CPPAN_BUILD
    EVP_MD_CTX_cleanup(ctx.get());
#else
    EVP_MD_CTX_destroy(ctx);
#endif
}

void DownloadData::finalize()
{
    md5.finalize();
    sha256.finalize();
}

DownloadData::DownloadData()
{
    md5.hash_function = &EVP_md5;
    sha256.hash_function = &EVP_sha256;
}

size_t DownloadData::progress(char *ptr, size_t size, size_t nmemb)
{
    auto read = size * nmemb;
    ofile->write(ptr, read);
    md5.progress(ptr, size, nmemb);
    sha256.progress(ptr, size, nmemb);
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
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res == CURLE_ABORTED_BY_CALLBACK)
    {
        fs::remove(data.fn);
        throw std::runtime_error("File '" + data.url + "' is too big. Limit is " + std::to_string(data.file_size_limit) + " bytes.");
    }
    if (res != CURLE_OK)
        throw std::runtime_error("curl error: "s + curl_easy_strerror(res));

    if (http_code / 100 != 2)
        throw std::runtime_error("Http returned " + std::to_string(http_code));
}

String download_file(const String &url)
{
    DownloadData dd;
    dd.url = url;
    dd.file_size_limit = 1'000'000'000;
    dd.fn = get_temp_filename();
    download_file(dd);
    auto s = read_file(dd.fn);
    fs::remove(dd.fn);
    return s;
}

HttpResponse url_request(const HttpRequest &request)
{
    auto curl = curl_easy_init();

    if (request.verbose)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

    if (!request.agent.empty())
        curl_easy_setopt(curl, CURLOPT_USERAGENT, request.agent.c_str());
    if (!request.username.empty())
        curl_easy_setopt(curl, CURLOPT_USERNAME, request.username.c_str());
    if (!request.password.empty())
        curl_easy_setopt(curl, CURLOPT_USERPWD, request.password.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    if (request.connect_timeout != -1)
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.connect_timeout);
    if (request.timeout != -1)
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout);

    // proxy settings
    auto proxy_addr = getAutoProxy();
    if (!proxy_addr.empty())
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_addr.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    }
    if (!request.proxy.host.empty())
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, request.proxy.host.c_str());
        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
        if (!request.proxy.user.empty())
            curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, request.proxy.user.c_str());
    }

    switch (request.type)
    {
        case HttpRequest::POST:
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.data.c_str());
            break;
#undef DELETE
        case HttpRequest::DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
    }

    if (request.url.find("https") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
        if (request.ignore_ssl_checks)
        {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
            //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0);
        }
    }

    HttpResponse response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_string);

    auto res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        throw std::runtime_error("curl error: "s + curl_easy_strerror(res));

    return response;
}

bool isUrl(const String &s)
{
    if (s.find("http://") == 0 ||
        s.find("https://") == 0 ||
        s.find("ftp://") == 0 ||
        s.find("git://") == 0 ||
        // could be dangerous in case of vulnerabilities on client side?
        //s.find("ssh://") == 0 ||
        0
        )
    {
        return true;
    }
    return false;
}

bool isValidSourceUrl(const String &url)
{
    if (url.empty())
        return false;
    if (!isUrl(url))
        return false;
    if (url.find_first_of(R"bbb('"`\|;$ @!#^*()<>[],)bbb") != url.npos)
        return false;
    // remove? will fail: ssh://name:pass@web.site
    if (std::count(url.begin(), url.end(), ':') > 1)
        return false;
    if (url.find("&&") != url.npos)
        return false;
#ifndef CPPAN_TEST
    if (url.find("file:") == 0) // prevent loading local files
        return false;
#endif
    for (auto &c : url)
    {
        if (c < 0 || c > 127)
            return false;
    }
    return true;
}

void checkSourceUrl(const String &url)
{
    if (!isValidSourceUrl(url))
        throw std::runtime_error("Bad source url: " + url);
}
