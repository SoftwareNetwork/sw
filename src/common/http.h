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

#pragma once

#include <openssl/evp.h>

#include "cppan_string.h"
#include "filesystem.h"
#include "property_tree.h"

struct ProxySettings
{
    String host;
    String user;
};

String getAutoProxy();

struct HttpSettings
{
    bool verbose = false;
    bool ignore_ssl_checks = false;
    ProxySettings proxy;
};

extern HttpSettings httpSettings;

struct HttpRequest : public HttpSettings
{
#undef DELETE
    enum Type
    {
        GET,
        POST,
        DELETE
    };

    String url;
    String agent;
    String username;
    String password;
    int type = GET;
    String data;
    int timeout = -1;
    int connect_timeout = -1;

    HttpRequest(const HttpSettings &parent)
        : HttpSettings(parent)
    {}
};

struct HttpResponse
{
    long http_code = 0;
    String response;
};

struct DownloadData
{
    struct Hasher
    {
        String *hash = nullptr;
        const EVP_MD *(*hash_function)(void) = nullptr;

        ~Hasher();
        void finalize();
        void progress(char *ptr, size_t size, size_t nmemb);

    private:
#ifndef CPPAN_BUILD
        std::unique_ptr<EVP_MD_CTX> ctx;
#else
        EVP_MD_CTX *ctx = nullptr;
#endif
    };

    String url;
    path fn;
    int64_t file_size_limit = 1 * 1024 * 1024;
    Hasher md5;
    Hasher sha256;

    // service
    std::ofstream *ofile = nullptr;

    DownloadData();

    void finalize();
    size_t progress(char *ptr, size_t size, size_t nmemb);
};

HttpResponse url_request(const HttpRequest &settings);
void download_file(DownloadData &data);
String download_file(const String &url);

bool isUrl(const String &s);
bool isValidSourceUrl(const String &url);
void checkSourceUrl(const String &url);
