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

#pragma once

#include <openssl/evp.h>

#include "common.h"
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
        EVP_MD_CTX *ctx;
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
