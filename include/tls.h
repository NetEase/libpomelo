#ifndef PC_TLS_H
#define PC_TLS_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include "pomelo.h"

struct pc_tls_s {
  SSL *ssl;
	SSL_CTX *ssl_ctx;

	const char *tls_cafile;
	const char *tls_capath;

	const char *tls_certfile;
	const char *tls_keyfile;
	int (*tls_pw_callback)(char *buf, int size, int rwflag, void *userdata);

	int enable_cert_verify;
	const char *tls_ciphers;
	int (*hostname_verify_cb)(pc_client_t*, const char** names, int len);

  BIO* in;
  BIO* out;
};

int pc_tls_init(pc_client_t* client);
int pc_tls_clear(pc_client_t* client);

#endif /* PC_TLS_H */ 
