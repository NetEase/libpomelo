#include "pomelo.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "tls.h"

#include <openssl/x509v3.h>

#define MAX_SAN_COUNT 32

static int _pc_verify_cert_hostname(X509* cert, pc_client_t* client);
static int _pc_server_cert_verify(int ok, X509_STORE_CTX* ctx);
static unsigned int _pc_psk_client_callback(SSL* ssl, const char* hint, 
        char *identity, unsigned int max_identity_len, unsigned char* psk, unsigned int max_psk_len);

static unsigned int _pc_psk_client_callback(SSL* ssl, const char* hint, 
        char *identity, unsigned int max_identity_len, unsigned char* psk, unsigned int max_psk_len) 
{
    BIGNUM* bn = 0;
    int len;

    pc_client_t* client = (pc_client_t*)SSL_get_app_data(ssl);
    if (!client || !client->tls || !client->tls->tls_psk_identity) return 0; 
    snprintf(identity, max_identity_len, "%s", client->tls->tls_psk_identity);

    if(!BN_hex2bn(&bn, client->tls->tls_psk)) {
        if(bn) BN_free(bn);
        return 0;
    }
    if(BN_num_bytes(bn) > max_identity_len) {
        BN_free(bn);
        return 0;
    }
    len = BN_bn2bin(bn, psk);
    BN_free(bn);
    return len;
}

static int _pc_server_cert_verify(int ok, X509_STORE_CTX* ctx) {
    SSL* ssl;
    X509* cert;
    pc_client_t* client;

    if (!ok) return 0;

    ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    client = SSL_get_app_data(ssl);
    
    if (!client || !client->tls) return 0;

    if (!client->tls->hostname_verify_cb) return ok;

    if (X509_STORE_CTX_get_error_depth(ctx) == 0) {
        cert = X509_STORE_CTX_get_current_cert(ctx);
        return _pc_verify_cert_hostname(cert, client);
    }

    return ok;
}

static int _pc_verify_cert_hostname(X509* cert, pc_client_t* client) 
{
    int i;
    int ret;
    char name[256] = {0};
    X509_NAME *subj;

    const char** names = (const char**) malloc(sizeof(const char*) * MAX_SAN_COUNT);
    int name_cnt = 0;

    STACK_OF(GENERAL_NAME)* san;
    const GENERAL_NAME* nval;
    const unsigned char* data;

    /* get all the subject alt names from cert */
    san = X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0);
    if (san) {
        for (i = 0; i < sk_GENERAL_NAME_num(san); ++i) {
            nval = sk_GENERAL_NAME_value(san, i);
            if (nval->type == GEN_DNS) {
                names[name_cnt++] = strdup(ASN1_STRING_data(nval->d.dNSName));
            } else if (nval->type == GEN_IPADD) {
                data = ASN1_STRING_data(nval->d.iPAddress);
                memset(name, 0, sizeof(name));
                if (nval->d.iPAddress->length == 4) {
#if defined(WIN32)
                    InetNtop(AF_INET, data, name, 4);
#else
                    inet_ntop(AF_INET, data, name, 4);
#endif
                } else if (nval->d.iPAddress->length  == 16) {
#if defined(WIN32)
                    InetNtop(AF_INET6, data, name, 16);
#else
                    inet_ntop(AF_INET6, data, name, 16);
#endif
                }
                names[name_cnt++] = strdup(name);
            }
        } /* end for */
    }

    subj = X509_get_subject_name(cert);
    memset(name, 0, sizeof(name));
    if (X509_NAME_get_text_by_NID(subj, NID_commonName, name, sizeof(name)) > 0) {
        name[sizeof(name) - 1] = 0;
        names[name_cnt++] = strdup(name);
    }

    ret = client->tls->hostname_verify_cb(client, names,  name_cnt);

    for(i = 0; i < name_cnt; ++i) {
        free((void*)names[i]);
    }
    free(names);
    return ret;
}

static const char* test_and_dup_filename(const char* filename)
{
    FILE *fptr;
    const char* ret = 0;

    fptr = fopen(filename, "rt");
    if (!fptr)
        return 0;

    fclose(fptr);
    return strdup(filename);
}

int pc_client_set_tls_ca(pc_client_t* client,
        const char *cafile, const char *capath) 
{
    FILE *fptr;
    if(!client || !client->tls || (!cafile && !capath)) return -1;

    client->tls->tls_cafile = test_and_dup_filename(cafile);
    client->tls->tls_capath = strdup(capath);

    if (!client->tls->tls_cafile && !client->tls->tls_capath) {
        return -1;
    }

    return 0;
}

int pc_client_set_tls_cert(pc_client_t* client, const char *certfile, const char *keyfile,
        int (*pw_callback)(char *buf, int size, int rwflag, void *userdata)) 
{
    if(!client || !client->tls || !keyfile || !certfile) return -1;
    client->tls->tls_certfile = test_and_dup_filename(certfile);
    if (!client->tls->tls_certfile) {
        free((void*)client->tls->tls_keyfile);
        client->tls->tls_keyfile = 0;
        return -1;
    }

    client->tls->tls_keyfile = test_and_dup_filename(keyfile);
    if (!client->tls->tls_keyfile) {
        free((void*)client->tls->tls_certfile);
        client->tls->tls_certfile = 0;
        return -1;
    }

    client->tls->tls_pw_callback = pw_callback;
    return 0;
}


int pc_client_set_tls_opts(pc_client_t* client, int verify, const char* ciphers)
{
    if(!client || !client->tls) return -1;

    client->tls->enable_cert_verify = verify;
    if (ciphers) {
        client->tls->tls_ciphers = strdup(ciphers);
        if(!client->tls->tls_ciphers) return -1;
    }else{
        free((void*)client->tls->tls_ciphers);
        client->tls->tls_ciphers = 0;
    }
}

int pc_client_set_tls_hostname_verify(pc_client_t* client, int (*verify_cb)(pc_client_t* client, const char** names, int len)) {
    if (!client || !client->tls) return -1;

    client->tls->hostname_verify_cb = verify_cb;
    return 0;
}


int pc_client_set_tls_psk(pc_client_t* client, const char* psk, const char* identity) {
    if (!client || !client->tls) return -1;

    if (strspn(psk, "0123456789abcdefABCDEF") < strlen(psk)) {
        /*psk is invalid*/
        return -1;
    }
    client->tls->tls_psk = strdup(psk);
    if (!client->tls->tls_psk) return -1;

    client->tls->tls_psk_identity = strdup(identity);
    if (!client->tls->tls_psk_identity) {
        free((void*)client->tls->tls_psk);
        client->tls->tls_psk = 0;
        return -1;
    }
    return 0;
}

int pc_client_lib_init()
{
    srand(time(0));
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    return 0;
}

int pc_client_lib_cleanup() 
{
    CRYPTO_cleanup_all_ex_data();
    EVP_cleanup();
    ERR_free_strings();
    return 0;
}

int pc_tls_init(pc_client_t* client) 
{
    pc_tls_t* tls;
    int ret;
    if (!client->tls) 
        return -1;
    tls = client->tls;

    tls->ssl_ctx = SSL_CTX_new(TLSv1_2_client_method());

    if (!tls->ssl_ctx) {
        return -1;
    }
    SSL_CTX_set_mode(tls->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);

    if (tls->tls_ciphers) {
        ret = SSL_CTX_set_cipher_list(tls->ssl_ctx, tls->tls_ciphers);
        if (!ret) {
            return -1;
        }
    }

    if (tls->tls_psk && !tls->tls_cafile && !tls->tls_capath) {
        SSL_CTX_set_psk_client_callback(tls->ssl_ctx, _pc_psk_client_callback);
        goto next;
    }

    if (tls->tls_cafile || tls->tls_capath) {
        ret = SSL_CTX_load_verify_locations(tls->ssl_ctx, tls->tls_cafile, tls->tls_capath);
        if (!ret) {
            return -1;
        }
    }

    if (tls->enable_cert_verify) {
        SSL_CTX_set_verify(tls->ssl_ctx, SSL_VERIFY_PEER, _pc_server_cert_verify); 
    } else {
        SSL_CTX_set_verify(tls->ssl_ctx, SSL_VERIFY_NONE, NULL);
    }

    if (tls->tls_pw_callback) {
        SSL_CTX_set_default_passwd_cb(tls->ssl_ctx, tls->tls_pw_callback);
        SSL_CTX_set_default_passwd_cb_userdata(tls->ssl_ctx, client);
    }

    if (tls->tls_certfile && tls->tls_keyfile) {
        ret = SSL_CTX_use_certificate_chain_file(tls->ssl_ctx, tls->tls_certfile);
        if (ret == 0) {
            return -1;
        }
        ret = SSL_CTX_use_PrivateKey_file(tls->ssl_ctx, tls->tls_keyfile, SSL_FILETYPE_PEM);
        if (ret == 0) {
            return -1;
        }
        ret = SSL_CTX_check_private_key(tls->ssl_ctx);
        if (ret == 0) {
            return -1;
        }
    }
next:
    tls->ssl = SSL_new(tls->ssl_ctx);
    SSL_set_app_data(tls->ssl, client);
    tls->in = BIO_new(BIO_s_mem());
    tls->out = BIO_new(BIO_s_mem());
    SSL_set_bio(tls->ssl, tls->in, tls->out);
    return 0;
}
