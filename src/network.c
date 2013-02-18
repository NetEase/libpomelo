#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <pomelo-client.h>
#include <assert.h>
#include <pomelo-protocol/message.h>
#include <pomelo-private/common.h>

int pc__handshake_req(pc_client_t *client);

// private callback functions
static uv_buf_t pc__alloc_buffer(uv_handle_t *handle, size_t suggested_size);
static void pc__on_tcp_read(uv_stream_t *handle, ssize_t nread, uv_buf_t buf);
static void pc__on_tcp_connect(uv_connect_t *req, int status);
static void pc__on_notify(uv_write_t* req, int status);
static void pc__on_request(uv_write_t *req, int status);
static int pc__async_write(pc_client_t *client, pc_tcp_req_t *req,
                           const char *route, json_t *msg);
static void pc__async_write_cb(uv_async_t* handle, int status);
static void pc__notify(pc_notify_t *req, int status);
static void pc__request(pc_request_t *req, int status);

static uint32_t pc__req_id = 0;

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

int pc_request_init(pc_request_t *req) {
  memset(req, 0, sizeof(pc_request_t));
  req->type = PC_REQUEST;

  return 0;
}

int pc_request(pc_client_t *client, pc_request_t *req, const char *route,
               json_t *msg, pc_request_cb cb) {
  req->cb = cb;
  req->id = ++pc__req_id;
  return pc__async_write(client, (pc_tcp_req_t *)req, route, msg);
}

/**
 * Initiate notify request instance.
 */
int pc_notify_init(pc_notify_t *req) {
  memset(req, 0, sizeof(pc_notify_t));
  req->type = PC_NOTIFY;

  return 0;
}

/**
 * Send notify to server.
 */
int pc_notify(pc_client_t *client, pc_notify_t *req, const char *route,
               json_t *msg, pc_notify_cb cb) {
  req->cb = cb;
  return pc__async_write(client, (pc_tcp_req_t *)req, route, msg);
}

static void pc__request(pc_request_t *req, int status) {
  if(status == -1) {
    req->cb(req, status, NULL);
    return;
  }

  pc_client_t *client = req->client;
  // check client state again
  if(client->state != PC_ST_WORKING) {
    fprintf(stderr, "Fail to request for Pomelo client not connected.\n");
    req->cb(req, status, NULL);
    return;
  }

  pc_buf_t msg_buf, pkg_buf;
  uv_write_t * write_req = NULL;
  void** data = NULL;

  memset(&msg_buf, 0, sizeof(pc_buf_t));
  memset(&pkg_buf, 0, sizeof(pc_buf_t));

  msg_buf = pc_msg_encode(req->id, req->route, req->msg, client->route_to_code,
                          client->pb_map);

  if(msg_buf.len == -1) {
    fprintf(stderr, "Fail to encode request message.\n");
    goto error;
  }

  pkg_buf = pc_pkg_encode(PC_PKG_DATA, msg_buf.base, msg_buf.len);

  if(msg_buf.len == -1) {
    fprintf(stderr, "Fail to encode request package.\n");
    goto error;
  }

  write_req = (uv_write_t *)malloc(sizeof(uv_write_t));

  if(write_req == NULL) {
    goto error;
  }

  memset(write_req, 0, sizeof(uv_write_t));

  // record request context
  data = (void **)malloc(sizeof(void *) * 2);
  if(data == NULL) {
    fprintf(stderr, "Fail to malloc void** for pc__request.\n");
    goto error;
  }

  data[0] = (void *)req;
  data[1] = pkg_buf.base;
  write_req->data = (void *)data;

  int res = uv_write(write_req, (uv_stream_t *)&client->socket,
                     (uv_buf_t *)&pkg_buf, 1, pc__on_request);

  if(res == -1) {
    fprintf(stderr, "Send message error %s\n",
            uv_err_name(uv_last_error(write_req->handle->loop)));
    goto error;
  }

  char req_id_str[32];
  memset(req_id_str, 0, 32);
  sprintf(req_id_str, "%u", req->id);
  pc_map_set(&client->requests, req_id_str, req);

  assert(msg_buf.base);
  free(msg_buf.base);

  return;

error:
  if(msg_buf.len != -1) free(msg_buf.base);
  if(pkg_buf.len != -1) free(pkg_buf.base);
  if(write_req) free(write_req);
  if(data) free(data);
  req->cb(req, -1, NULL);
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
  if(client->state != PC_ST_WORKING) {
    fprintf(stderr, "Fail to notify for Pomelo client not connected.\n");
    req->cb(req, status);
    return;
  }

  pc_buf_t msg_buf;
  pc_buf_t pkg_buf;
  memset(&msg_buf, 0, sizeof(pc_buf_t));
  memset(&pkg_buf, 0, sizeof(pc_buf_t));

  uv_write_t * write_req = NULL;
  void** data = NULL;

  msg_buf = pc_msg_encode(0, req->route, req->msg, client->route_to_code,
                          client->pb_map);

  if(msg_buf.len == -1) {
    fprintf(stderr, "Fail to encode request message.\n");
    goto error;
  }

  pkg_buf = pc_pkg_encode(PC_PKG_DATA, msg_buf.base, msg_buf.len);

  if(msg_buf.len == -1) {
    fprintf(stderr, "Fail to encode request package.\n");
    goto error;
  }

  write_req = (uv_write_t *)malloc(sizeof(uv_write_t));

  if(write_req == NULL) {
    goto error;
  }

  memset(write_req, 0, sizeof(uv_write_t));

  // record notify context
  data = (void **)malloc(sizeof(void *) * 2);
  if(data == NULL) {
    fprintf(stderr, "Fail to malloc void** for pc__request.\n");
    goto error;
  }

  data[0] = (void *)req;
  data[1] = pkg_buf.base;
  write_req->data = (void *)data;

  int res = uv_write(write_req, (uv_stream_t *)&client->socket,
                     (uv_buf_t *)&pkg_buf, 1, pc__on_notify);

  if(res == -1) {
    fprintf(stderr, "Send message error %s\n",
            uv_err_name(uv_last_error(write_req->handle->loop)));
    goto error;
  }

  assert(msg_buf.base);
  free(msg_buf.base);

  return;

error:
  if(msg_buf.len != -1) free(msg_buf.base);
  if(pkg_buf.len != -1) free(pkg_buf.base);
  if(write_req) free(write_req);
  if(data) free(data);
  req->cb(req, -1);
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
  printf("nread: %lu\n", nread);
  printf("client state: %d\n", client->state);
  for(int i=0; i<nread; i++) {
    printf("%02x ", buf.base[i]);
  }
  printf("\n");
  if(PC_ST_CLOSED == client->state) {
    return;
  }
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
                           const char *route, json_t *msg) {
  assert(client->state == PC_ST_WORKING);
  if(client->state != PC_ST_WORKING) {
    fprintf(stderr, "Pomelo client not working: %d.\n", client->state);
    return -1;
  }
  if(!req || !route /*|| !msg*/) {
    fprintf(stderr, "Invalid tcp request.\n");
    return -1;
  }

  int async_inited = 0;
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

  async_inited = 1;

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
  if(async_inited) {
    // should not release the async_req before close callback
    uv_close((uv_handle_t *)async_req, pc__handle_close_cb);
  } else {
    if(async_req) free(async_req);
  }
  return -1;
}

static void pc__async_write_cb(uv_async_t* req, int status) {
  pc_tcp_req_t *tcp_req = (pc_tcp_req_t *)req->data;
  uv_close((uv_handle_t *)req, pc__handle_close_cb);
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
 * Request callback.
 */
static void pc__on_request(uv_write_t *req, int status) {
  void **data = (void **)req->data;
  pc_request_t *request_req = (pc_request_t *)data[0];
  pc_client_t *client = request_req->client;
  char *base = (char *)data[1];

  free(base);
  free(req);
  free(data);

  if(status == -1) {
    fprintf(stderr, "Request error %s\n",
            uv_err_name(uv_last_error(req->handle->loop)));
    request_req->cb(request_req, status, NULL);
    return;
  }
}

/**
 * Notify callback.
 */
static void pc__on_notify(uv_write_t *req, int status) {
  void **data = (void **)req->data;
  pc_notify_t *notify_req = (pc_notify_t *)data[0];
  pc_client_t *client = (pc_client_t *)notify_req->client;
  char *base = (char *)data[1];

  free(base);
  free(req);
  free(data);

  if(status == -1) {
    fprintf(stderr, "Notify error %s\n",
            uv_err_name(uv_last_error(req->handle->loop)));
  }

  notify_req->cb(notify_req, status);
}

int pc__binary_write(pc_client_t *client, const char *data, size_t len,
                            uv_write_cb cb) {
  uv_write_t *req = NULL;
  void **attach = NULL;

  req = (uv_write_t *)malloc(sizeof(uv_write_t));
  if(req == NULL) {
    fprintf(stderr, "Fail to malloc for uv_write_t.\n");
    goto error;
  }

  memset(req, 0, sizeof(uv_write_t));

  attach = (void **)malloc(sizeof(void *) * 2);
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