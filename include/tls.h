#ifndef PC_TLS_H
#define PC_TLS_H

#if defined(WITH_TLS)

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

	int tls_cert_verify;
	const char *tls_ciphers;
	int (*secure_cb)(pc_client_t*, const char** names, int len);

	const char *tls_psk;
	const char *tls_psk_identity;

  BIO* in;
  BIO* out;
};

#endif /* WITH_TLS */
#endif /* PC_TLS_H */ 
