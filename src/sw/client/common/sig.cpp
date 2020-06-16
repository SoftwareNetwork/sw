// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "sig.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <primitives/exceptions.h>

static const String sw_pubkey = R"(
-----BEGIN PUBLIC KEY-----
MFYwEAYHKoZIzj0CAQYFK4EEAAoDQgAEOg4zSPZYhB4cVx7nGXDpilsg+zjLbqvb
FOp5gc5dY1HE5ctavjo0xR01tU1Co/enuQIIHqUb+yWS2wqPT9T27w==
-----END PUBLIC KEY-----
)";

static void ds_sign_file(const path &fn, const path &pkey_fn)
{
    auto msg = read_file(fn);
    auto key = read_file(pkey_fn);
    uint8_t *sig = nullptr;
    size_t *slen = 0;

    BIO* bo = BIO_new( BIO_s_mem() );
    BIO_write( bo, key.c_str(), key.size());

    EVP_PKEY* pkey = 0;
    PEM_read_bio_PrivateKey( bo, &pkey, 0, 0 );

    BIO_free(bo);

    EVP_MD_CTX *mdctx = NULL;
    int ret = 0;

    /* Create the Message Digest Context */
    if(!(mdctx = EVP_MD_CTX_create())) goto err;

    /* Initialise the DigestSign operation */
    if(1 != EVP_DigestSignInit(mdctx, NULL, EVP_sha512(), NULL, pkey)) goto err;

    /* Call update with the message */
    if(1 != EVP_DigestSignUpdate(mdctx, msg.c_str(), msg.size())) goto err;

    /* Finalise the DigestSign operation */
    /* First call EVP_DigestSignFinal with a NULL sig parameter to obtain the length of the
    * signature. Length is returned in slen */
    if(1 != EVP_DigestSignFinal(mdctx, NULL, slen)) goto err;
    /* Allocate memory for the signature based on size in slen */
    if(!(sig = (uint8_t *)OPENSSL_malloc(sizeof(unsigned char) * (*slen)))) goto err;
    /* Obtain the signature */
    if(1 != EVP_DigestSignFinal(mdctx, sig, slen)) goto err;

    /* Success */
    ret = 1;

err:
    if(ret != 1)
    {
        /* Do some error handling */
    }

    /* Clean up */
    if(sig && !ret) OPENSSL_free(sig);
    if(mdctx) EVP_MD_CTX_destroy(mdctx);
}

static auto algoFromString(const path &ext)
{
    if (0);
#define CASE(x) else if (ext == #x) return EVP_ ## x ()
    CASE(sha256);
    CASE(sha512);

    throw SW_RUNTIME_ERROR("Unknown signature algorithm: " + ext.string());
}

static auto algoFromExtension(const path &ext)
{
    return algoFromString(ext.string().substr(1));
}

static void ds_verify_file(const path &fn, const EVP_MD *algo, const String &sig, const String &pubkey)
{
    auto msg = read_file(fn);

    EVP_MD_CTX *mdctx = NULL;
    int ret = 0;

    BIO* bo = BIO_new(BIO_s_mem());
    BIO_write(bo, pubkey.c_str(), pubkey.size());

    EVP_PKEY* key = 0;
    PEM_read_bio_PUBKEY(bo, &key, 0, 0);
    BIO_free(bo);

    /* Create the Message Digest Context */
    if (!(mdctx = EVP_MD_CTX_create()))
        throw SW_RUNTIME_ERROR("Cannot create EVP MD context");

    /* Initialize `key` with a public key */
    if (1 != EVP_DigestVerifyInit(mdctx, NULL, algo, NULL, key))
    {
        EVP_MD_CTX_destroy(mdctx);
        throw SW_RUNTIME_ERROR("EVP_DigestVerifyInit() failed");
    }

    /* Initialize `key` with a public key */
    if (1 != EVP_DigestVerifyUpdate(mdctx, msg.data(), msg.size()))
    {
        EVP_MD_CTX_destroy(mdctx);
        throw SW_RUNTIME_ERROR("EVP_DigestVerifyUpdate() failed");
    }

    if (1 == EVP_DigestVerifyFinal(mdctx, (uint8_t*)sig.data(), sig.size()))
    {
        /* Success */
        EVP_MD_CTX_destroy(mdctx);
    }
    else
    {
        /* Failure */
        EVP_MD_CTX_destroy(mdctx);
        throw SW_RUNTIME_ERROR("Bad digital signature");
    }
}

static void ds_verify_file(const path &fn, const path &sigfn, const String &pubkey)
{
    ds_verify_file(fn, algoFromExtension(sigfn.stem().extension().string()), read_file(sigfn), pubkey);
}

static void ds_verify_file(const path &fn, const path &sigfn, const path &pubkey_fn)
{
    ds_verify_file(fn, sigfn, read_file(pubkey_fn));
}

void ds_verify_sw_file(const path &fn, const String &algo, const String &sig)
{
    ds_verify_file(fn, algoFromString(algo), sig, sw_pubkey);
}

