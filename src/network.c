#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <pomelo-client.h>

int pc__handshake_req(pc_client_t *client);

// private callback functions
static uv_buf_t pc__alloc_buffer(uv_handle_t *handle, size_t suggested_size);
static void pc__on_tcp_read(uv_stream_t *handle, ssize_t nread, uv_buf_t buf);
static void pc__on_tcp_connect(uv_connect_t *req, int status);
static void pc__on_notify(uv_write_t* req, int status);
static int pc__async_write(pc_client_t *client, pc_tcp_req_t *req,
                           const char *route, pc_message_t *msg);
static void pc__async_write_cb(uv_async_t* handle, int status);
static void pc__notify(pc_notify_t *req, int status);
static void pc__request(pc_request_t *req, int status);

/**
 * Initiate connect request instance.
 */
int pc_connect_init(pc_connect_t *req, struct sockaddr_in *address) {
  if(!req) {
    fprintf(stderr, "Invalid pc_connect_t to init.\n");
    return -1;
  }

  memset(req, 0, sizeof(pc_request_t));
  req->type = PC_CONNECT;
  req->address = address;

  return 0;
}

/**
 * Connect the pomelo client to server.
 */
int pc_connect(pc_client_t *client, pc_connect_t *req,
               json_t *handshake_opts, pc_connect_cb cb) {
  if(client->state != PC_ST_INITED) {
    fprintf(stderr, "Invalid Pomelo client state: %d.\n", client->state);
    return -1;
  }

  if(!req || !req->address) {
    fprintf(stderr, "Invalid connect request.\n");
    return -1;
  }

  uv_connect_t *connect_req = NULL;

  connect_req = (uv_connect_t *)malloc(sizeof(uv_connect_t));
  if(connect_req == NULL) {
    fprintf(stderr, "Fail to malloc for uv_connect_t.\n");
    return -1;
  }


  uv_tcp_init(client->uv_loop, &client->socket);

  client->handshake_opts = handshake_opts;
  client->conn_req = req;
  req->client = client;
  req->cb = cb;
  connect_req->data = (void *)req;

  int res = uv_tcp_connect(connect_req, &client->socket,
                           *req->address, pc__on_tcp_connect);

  if(res) {
    free(connect_req);
  }

  return res;
}

int pc_request_init(pc_request_t *req, const char *route, pc_message_t *msg) {
  if(!req) {
    fprintf(stderr, "Invalid request to init.\n");
    return -1;
  }

  memset(req, 0, sizeof(pc_request_t));
  req->type = PC_REQUEST;
  req->route = route;
  req->msg = msg;

  return 0;
}

int pc_request(pc_client_t *client, pc_request_t *req, pc_request_cb cb) {
  return 0;
}

/**
 * Initiate notify request instance.
 */
int pc_notify_init(pc_notify_t *req) {
  if(!req) {
    fprintf(stderr, "Invalid notify to init.\n");
    return -1;
  }

  memset(req, 0, sizeof(pc_notify_t));
  req->type = PC_NOTIFY;

  return 0;
}

/**
 * Send notify to server.
 */
int pc_notify(pc_client_t *client, pc_notify_t *req, const char *route,
               pc_message_t *msg, pc_notify_cb cb) {
  req->cb = cb;
  return pc__async_write(client, (pc_tcp_req_t *)req, route, msg);
}

/**
 * Async callback and send notify to server finally.
 */
static void pc__notify(pc_notify_t *req, int status) {
  if(status == -1) {
    req->cb(req, status);
    return;
  }

  pc_client_t *client = req->client;
  // check client state again
  if(client->state != PC_ST_CONNECTED) {
    fprintf(stderr, "Fail to notify for Pomelo client not connected.\n");
    req->cb(req, status);
    return;
  }

  // TODO: message encode
  uv_buf_t buf;

  if(buf.len == -1) {
    fprintf(stderr, "Fail to encode notify message.\n");
    goto error;
  }

  uv_write_t * write_req = (uv_write_t *)malloc(sizeof(uv_write_t));

  if(write_req == NULL) {
    goto error;
  }

  memset(write_req, 0, sizeof(uv_write_t));

  // record notify context
  write_req->data = (void *)req;

  int res = uv_write(write_req, (uv_stream_t *)&client->socket,
                     &buf, 1, pc__on_notify);

  if(res == -1) {
    fprintf(stderr, "Send message error %s\n",
            uv_err_name(uv_last_error(write_req->handle->loop)));
    goto error;
  }

  return;

error:
  if(buf.len != -1) free(buf.base);
  if(write_req) free(write_req);
  req->cb(req, -1);
}

static void pc__request(pc_request_t *req, int status) {

}

/**
 * Allocate buffer callback for uv_read_start.
 */
static uv_buf_t pc__alloc_buffer(uv_handle_t *handle, size_t suggested_size) {
  return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

/**
 * Tcp data reached callback.
 */
static void pc__on_tcp_read(uv_stream_t *socket, ssize_t nread, uv_buf_t buf) {
  pc_client_t *client = (pc_client_t *)socket->data;
  if (nread == -1) {
    if (uv_last_error(socket->loop).code != UV_EOF)
      fprintf(stderr, "Read error %s\n",
              uv_err_name(uv_last_error(socket->loop)));

    pc_disconnect((pc_client_t *)socket->data);
    return;
  }

  if(nread == 0) {
    // read noting
    free(buf.base);
    return;
  }

  if(pc_pkg_parser_feed(&client->pkg_parser, buf.base, nread)) {
    fprintf(stderr, "Fail to process data from server.\n");
    pc_disconnect(client);
  }
}

/**
 * Tcp connection established callback.
 */
static void pc__on_tcp_connect(uv_connect_t *req, int status) {
  pc_connect_t *conn_req = (pc_connect_t *)req->data;
  pc_client_t *client = conn_req->client;

  if (status == -1) {
    fprintf(stderr, "Connect failed error %s\n",
            uv_err_name(uv_last_error(req->handle->loop)));
    free(req);
    conn_req->cb(conn_req, status);
    return;
  }

  free(req);

  client->socket.data = (void *)client;

  // start the tcp reading until disconnect
  if(uv_read_start((uv_stream_t*)&client->socket, pc__alloc_buffer, pc__on_tcp_read)) {
    fprintf(stderr, "Fail to start reading server %s\n",
            uv_err_name(uv_last_error(client->uv_loop)));
    pc_disconnect(client);
    conn_req->cb(conn_req, -1);
    return;
  }

  client->state = PC_ST_CONNECTED;

  if(pc__handshake_req(client)) {
    pc_disconnect(client);
    status = -1;
  }
}

// Async write for pc_notify or pc_request may be invoked in other threads.
static int pc__async_write(pc_client_t *client, pc_tcp_req_t *req,
                           const char *route, pc_message_t *msg) {
  if(client->state != PC_ST_CONNECTED) {
    fprintf(stderr, "Pomelo client not connected.\n");
    return -1;
  }
  if(!req || !route /*|| !msg*/) {
    fprintf(stderr, "Invalid tcp request.\n");
    return -1;
  }

  req->client = client;
  req->route = route;
  req->msg = msg;

  uv_async_t *async_req = (uv_async_t *)malloc(sizeof(uv_async_t));
  if(async_req == NULL) {
    fprintf(stderr, "Fail to malloc for async notify.\n");
    goto error;
  }

  if(uv_async_init(client->uv_loop, async_req, pc__async_write_cb)) {
    fprintf(stderr, "Fail to init async write tcp request, type: %d.\n",
            req->type);
    goto error;
  }

  async_req->data = (void *)req;
  if(uv_async_send(async_req)) {
    fprintf(stderr, "Fail to send async write tcp request, type: %d.\n",
            req->type);
    goto error;
  }

  return 0;

error:
  if(req->route) {
    free((void *)req->route);
  }
  if(async_req) free(async_req);
  return -1;
}

static void pc__async_write_cb(uv_async_t* req, int status) {
  pc_tcp_req_t *tcp_req = (pc_tcp_req_t *)req->data;
  free(req);
  if(tcp_req->type == PC_NOTIFY) {
    pc__notify((pc_notify_t *)tcp_req, status);
  } else if(tcp_req->type == PC_REQUEST) {
    pc__request((pc_request_t *)tcp_req, status);
  } else {
    fprintf(stderr, "Unknown tcp request type: %d\n", tcp_req->type);
    // TDOO: should abort? How to free unknown tcp request
    free(tcp_req);
    return;
  }
}

/**
 * Notify callback.
 */
static void pc__on_notify(uv_write_t *req, int status) {
  pc_notify_t *notify_req = (pc_notify_t *)req->data;
  pc_client_t *client = (pc_client_t *)notify_req->client;
  char *base = (char *)notify_req->data;

  free(base);
  free(req);

  if(status == -1) {
    fprintf(stderr, "Notify error %s\n",
            uv_err_name(uv_last_error(req->handle->loop)));
  }

  notify_req->cb(notify_req, status);
}

int pc__binary_write(pc_client_t *client, const char *data, size_t len,
                            uv_write_cb cb) {
  uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
  if(req == NULL) {
    fprintf(stderr, "Fail to malloc for uv_write_t.\n");
    goto error;
  }

  memset(req, 0, sizeof(uv_write_t));

  void **attach = (void **)malloc(sizeof(void *) * 2);
  if(data == NULL) {
    fprintf(stderr, "Fail to malloc data for handshake ack.\n");
    goto error;
  }

  attach[0] = client;
  attach[1] = (void *)data;
  req->data = (void *)attach;

  uv_buf_t buf = {
    .base = (char *)data,
    .len = len
  };

  if(uv_write(req, (uv_stream_t *)&client->socket, &buf, 1, cb)) {
    fprintf(stderr, "Fail to write handshake ack pakcage, %s\n",
            uv_err_name(uv_last_error(req->handle->loop)));
    goto error;
  }

  return 0;
error:
  if(req) free(req);
  if(attach) free(attach);
  return -1;
}