#include "pomelo.h"
#include <stdio.h>
#include <string.h>

#ifdef WITH_TLS
#include "tls.h"

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
        free(client->tls->tls_keyfile);
        client->tls->tls_keyfile = 0;
        return -1;
    }

    client->tls->tls_keyfile = test_and_dup_filename(keyfile);
    if (!client->tls->tls_keyfile) {
        free(client->tls->tls_certfile);
        client->tls->tls_certfile = 0;
        return -1;
    }

    client->tls->tls_pw_callback = pw_callback;
    return 0;
}


int pc_client_set_tls_opts(pc_client_t* client, int verify, const char* ciphers)
{
    if(!client || !client->tls) return -1;

    client->tls->tls_cert_verify = verify;
    if (ciphers) {
        client->tls->tls_ciphers = strdup(ciphers);
        if(!client->tls->tls_ciphers) return -1;
    }else{
        free(client->tls->tls_ciphers);
        client->tls->tls_ciphers = 0;
    }
}

int pc_client_set_tls_secure(pc_client_t* client, int (*secure_cb)(pc_client_t* client, const char** names, int len)) {
    if (!client || !client->tls) return -1;

    client->tls->secure_cb = secure_cb;
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
        free(client->tls->tls_psk);
        client->tls->tls_psk = 0;
        return -1;
    }
    return 0;
}

#endif
