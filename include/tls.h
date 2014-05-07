#ifndef PC_TLS_H
#define PC_TLS_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include <pomelo-private/ngx-queue.h>

#include "pomelo.h"

typedef struct pc_tls_write_s pc_tls_write_t;

typedef void (*pc_tls_write_cb)(pc_tls_write_t* tls_write, int status);

struct pc_tls_write_t {
  pc_tcp_req_t* req;
  pc_tls_write_cb cb;
  ngx_queue_t queue;
};

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
  BIO* write_buf;
  uv_write_t write_req;
  int write_size;

  ngx_queue_t write_queue;
  ngx_queue_t pending_queue;
};

int pc_tls_init(pc_client_t* client);
int pc_tls_clear(pc_client_t* client);

int pc_tls_enc_out(pc_client* client);
int pc_tls_clear_in(pc_client* client);
int pc_tls_clear_out(pc_client* client);

int pc_tls_read_cb(uv_stream_t* stream, size_t* nread, const uv_buf_t* buf, uv_handle_type pending);

#endif /* PC_TLS_H */ 
