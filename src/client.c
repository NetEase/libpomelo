#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <pomelo-client.h>
#include <pomelo-private/listener.h>
#include <pomelo-protocol/message.h>
#include <pomelo-private/transport.h>

static uv_loop_t *global_uv_loop = NULL;

void pc__client_init(pc_client_t *client);
void pc_emit_event(pc_client_t *client, const char *event, void *data);
static void pc__pkg_cb(pc_pkg_type type, const char *data, size_t len,
                       void *attach);
int pc__handshake_resp(pc_client_t *client, const char *data, size_t len);
int pc__heartbeat(pc_client_t *client);
int pc__data(pc_client_t *client, const char *data, size_t len);
void pc__heartbeat_cb(uv_timer_t* heartbeat_timer, int status);
void pc__timeout_cb(uv_timer_t* timeout_timer, int status);
void pc__process_response(pc_client_t *client, pc_msg_t *msg);
pc_msg_t *pc__default_msg_parse_cb(pc_client_t *client,
    const char *data, int len);
void pc__default_msg_parse_done_cb(pc_client_t *client, pc_msg_t *msg);
pc_buf_t pc__default_msg_encode_cb(pc_client_t *client, uint32_t reqId,
    const char *route, json_t *msg);
void pc__default_msg_encode_done_cb(pc_client_t * client, pc_buf_t buf);
void pc__close_async_cb(uv_async_t *handle, int status);
void pc__cond_wait(pc_client_t *client, uint64_t timeout);
void pc__cond_broadcast(pc_client_t *client);
void pc__client_connected_cb(pc_connect_t* req, int status);
void pc__worker(void *arg);

static inline const char *pc__resolve_dictionary(pc_client_t *client,
    uint16_t code);

void pc__release_listeners(pc_map_t *map, const char* key, void *value);
void pc__release_requests(pc_map_t *map, const char* key, void *value);

pc_client_t *pc_client_new() {
  pc_client_t *client = (pc_client_t *)malloc(sizeof(pc_client_t));

  if(!client) {
    fprintf(stderr, "Fail to malloc for pc_client_t.\n");
    abort();
  }

  memset(client, 0, sizeof(pc_client_t));

  client->uv_loop = uv_loop_new();
  if(client->uv_loop == NULL) {
    fprintf(stderr, "Fail to create uv_loop_t.\n");
    abort();
  }

  pc__client_init(client);

  return client;
}

void pc__client_init(pc_client_t *client) {
  client->listeners = pc_map_new(pc__release_listeners);
  if(client->listeners == NULL) {
    fprintf(stderr, "Fail to init client->listeners.\n");
    abort();
  }

  client->requests = pc_map_new(pc__release_requests);
  if(client->requests == NULL) {
    fprintf(stderr, "Fail to init client->requests.\n");
    abort();
  }

  client->pkg_parser = pc_pkg_parser_new(pc__pkg_cb, client);
  if(client->pkg_parser == NULL) {
    fprintf(stderr, "Fail to init client->pkg_parser.\n");
    abort();
  }

  if(uv_timer_init(client->uv_loop, &client->heartbeat_timer)) {
    fprintf(stderr, "Fail to init client->heartbeat_timer.\n");
    abort();
  }
  client->heartbeat_timer.timer_cb = pc__heartbeat_cb;
  client->heartbeat_timer.data = client;

  if(uv_timer_init(client->uv_loop, &client->timeout_timer)) {
    fprintf(stderr, "Fail to init client->timeout_timer.\n");
    abort();
  }
  client->timeout_timer.timer_cb = pc__timeout_cb;
  client->timeout_timer.data = client;

  uv_async_init(client->uv_loop, &client->close_async, pc__close_async_cb);
  client->close_async.data = client;
  uv_mutex_init(&client->mutex);
  uv_cond_init(&client->cond);
  uv_mutex_init(&client->listener_mutex);

  // init package parser
  client->parse_msg = pc__default_msg_parse_cb;
  client->parse_msg_done = pc__default_msg_parse_done_cb;

  // init package encoder
  client->encode_msg = pc__default_msg_encode_cb;
  client->encode_msg_done = pc__default_msg_encode_done_cb;

  client->state = PC_ST_INITED;
}

/**
 * Clear all inner resource of Pomelo client
 */
void pc__client_clear(pc_client_t *client) {
  if(client->uv_loop) {
    free(client->uv_loop);
    client->uv_loop = NULL;
  }

  if(client->listeners) {
    pc_map_destroy(client->listeners);
    client->listeners = NULL;
  }

  if(client->requests) {
    pc_map_destroy(client->requests);
    client->requests = NULL;
  }

  if(client->pkg_parser) {
    pc_pkg_parser_destroy(client->pkg_parser);
    client->pkg_parser = NULL;
  }

  if(client->handshake_opts) {
    json_decref(client->handshake_opts);
    client->handshake_opts = NULL;
  }

  if(client->route_to_code) {
    json_decref(client->route_to_code);
    client->route_to_code = NULL;
  }
  if(client->code_to_route) {
    json_decref(client->code_to_route);
    client->code_to_route = NULL;
  }
  if(client->server_protos) {
    json_decref(client->server_protos);
    client->server_protos = NULL;
  }
  if(client->client_protos) {
    json_decref(client->client_protos);
    client->client_protos = NULL;
  }
}

void pc_client_stop(pc_client_t *client) {
  if(PC_ST_INITED == client->state || PC_ST_CONNECTING == client->state) {
    client->state = PC_ST_CLOSED;
    return;
  }

  if(PC_TP_ST_CLOSED == client->state) {
    return;
  }

  client->state = PC_ST_CLOSED;
  if(client->transport) {
    pc_transport_destroy(client->transport);
    client->transport = NULL;
  }

  uv_close((uv_handle_t *)&client->heartbeat_timer, NULL);
  uv_close((uv_handle_t *)&client->timeout_timer, NULL);
  uv_close((uv_handle_t *)&client->close_async, NULL);
}

void pc_client_destroy(pc_client_t *client) {
  if(PC_ST_INITED == client->state) {
    pc__client_clear(client);
    free(client);
    return;
  }

  if(PC_ST_CLOSED == client->state) {
    pc__client_clear(client);
    free(client);
    return;
  }

  // 1. asyn worker thread
  // 2. wait cond until signal or timeout
  // 3. free client
  uv_async_send(&client->close_async);

  pc__cond_wait(client, 3000);

  if(PC_ST_CLOSED != client->state) {
    pc_client_stop(client);
    pc__client_clear(client);
  }
  free(client);
}

int pc_client_join(pc_client_t *client) {
  if(PC_ST_WORKING != client->state) {
    fprintf(stderr, "Fail to join client for invalid state: %d.\n",
            client->state);
    return -1;
  }
  return uv_thread_join(&client->worker);
}

int pc_client_connect(pc_client_t *client, struct sockaddr_in *addr) {
  pc_connect_t *conn_req = pc_connect_req_new(addr);

  if(conn_req == NULL) {
    fprintf(stderr, "Fail to malloc pc_connect_t.\n");
    goto error;
  }

  if(pc_connect(client, conn_req, NULL, pc__client_connected_cb)) {
    fprintf(stderr, "Fail to connect to server.\n");
    goto error;
  }

  // 1. start work thread
  // 2. wait connect result

  uv_thread_create(&client->worker, pc__worker, client);

  // TODO should set a timeout?
  pc__cond_wait(client, 0);

  pc_connect_req_destroy(conn_req);

  if(PC_ST_WORKING != client->state) {
    return -1;
  }

  return 0;
error:
  if(conn_req) pc_connect_req_destroy(conn_req);
  return -1;
}

int pc_add_listener(pc_client_t *client, const char *event,
                    pc_event_cb event_cb) {
  if(PC_ST_CLOSED == client->state) {
    fprintf(stderr, "Pomelo client has closed.\n");
    return -1;
  }

  pc_listener_t *listener = pc_listener_new();
  if(listener == NULL) {
    fprintf(stderr, "Fail to create listener.\n");
    return -1;
  }
  listener->cb = event_cb;

  uv_mutex_lock(&client->mutex);
  ngx_queue_t *head = pc_map_get(client->listeners, event);

  if(head == NULL) {
    head = (ngx_queue_t *)malloc(sizeof(ngx_queue_t));
    if(head == NULL) {
      fprintf(stderr, "Fail to create listener queue.\n");
      pc_listener_destroy(listener);
      return -1;
    }

    ngx_queue_init(head);

    pc_map_set(client->listeners, event, head);
  }

  ngx_queue_insert_tail(head, &listener->queue);
  uv_mutex_unlock(&client->mutex);

  return 0;
}

void pc_remove_listener(pc_client_t *client, const char *event, pc_event_cb cb) {
  uv_mutex_lock(&client->mutex);
  ngx_queue_t *head = (ngx_queue_t *)pc_map_get(client->listeners, event);
  if(head == NULL) {
    return;
  }

  ngx_queue_t *item = NULL;
  pc_listener_t *listener = NULL;

  ngx_queue_foreach(item, head) {
    listener = ngx_queue_data(item, pc_listener_t, queue);
    if(listener->cb == cb) {
      ngx_queue_remove(item);
      pc_listener_destroy(listener);
      break;
    }
  }

  if(ngx_queue_empty(head)) {
    pc_map_del(client->listeners, event);
    free(head);
  }
  uv_mutex_unlock(&client->mutex);
}

void pc_emit_event(pc_client_t *client, const char *event, void *data) {
  uv_mutex_lock(&client->mutex);
  ngx_queue_t *head = (ngx_queue_t *)pc_map_get(client->listeners, event);
  if(head == NULL) {
    return;
  }

  ngx_queue_t *item = NULL;
  pc_listener_t *listener = NULL;

  ngx_queue_foreach(item, head) {
    listener = ngx_queue_data(item, pc_listener_t, queue);
    listener->cb(client, event, data);
  }
  uv_mutex_unlock(&client->mutex);
}

int pc_run(pc_client_t *client) {
  if(!client || !client->uv_loop) {
    fprintf(stderr, "Invalid client to run.\n");
    return -1;
  }
  return uv_run(client->uv_loop, UV_RUN_DEFAULT);
};

/**
 * New Pomelo package arrived callback.
 *
 * @param type   Pomelo package type
 * @param data   package body
 * @param len    package body length
 * @param attach attach pointer pass to pc_pkg_parser_t
 */
static void pc__pkg_cb(pc_pkg_type type, const char *data, size_t len,
                       void *attach) {
  pc_client_t *client = (pc_client_t *)attach;
  int status = 0;
  switch(type) {
    case PC_PKG_HANDSHAKE:
      status = pc__handshake_resp(client, data, len);
    break;
    case PC_PKG_HEARBEAT:
      status = pc__heartbeat(client);
    break;
    case PC_PKG_DATA:
      status = pc__data(client, data, len);
    break;
    default:
      fprintf(stderr, "Unknown Pomelo package type: %d.\n", type);
      status = -1;
    break;
  }

  if(status == -1) {
    pc_client_stop(client);
  }
}

int pc__data(pc_client_t *client, const char *data, size_t len) {
  pc_msg_t *msg = client->parse_msg(client, data, len);

  if(msg == NULL) {
    return -1;
  }

  if(msg->id > 0) {
    pc__process_response(client, msg);
  } else {
    // server push message
    pc_emit_event(client, msg->route, msg->msg);
  }

  client->parse_msg_done(client, msg);

  return 0;
}

void pc__process_response(pc_client_t *client, pc_msg_t *msg) {
  char req_id_str[64];
  memset(req_id_str, 0, 64);
  sprintf(req_id_str, "%u", msg->id);

  pc_request_t *req = (pc_request_t *)pc_map_get(client->requests, req_id_str);
  if(req == NULL) {
    fprintf(stderr, "Fail to get pc_request_t for request id: %u.\n", msg->id);
    return;
  }

  // clean request for the reqId
  pc_map_del(client->requests, req_id_str);

  req->cb(req, 0, msg->msg);
}

static inline const char *pc__resolve_dictionary(pc_client_t *client,
                                                 uint16_t code) {
  char code_str[8];
  memset(code_str, 0, 8);
  sprintf(code_str, "%d", code);
  return json_string_value(json_object_get(client->code_to_route, code_str));
}

pc_msg_t *pc__default_msg_parse_cb(pc_client_t *client, const char *data, int len) {
  const char *route_str = NULL;
  pc_msg_t *msg = NULL;

  pc__msg_raw_t *raw_msg = pc_msg_decode(data, len);
  if(raw_msg == NULL) {
    goto error;
  }

  msg = (pc_msg_t *)malloc(sizeof(pc_msg_t));
  if(msg == NULL) {
    fprintf(stderr, "Fail to malloc for pc_msg_t while parsing raw message.\n");
    goto error;
  }
  memset(msg, 0, sizeof(pc_msg_t));

  msg->id = raw_msg->id;

  // route
  if(PC_MSG_HAS_ROUTE(raw_msg->type)) {
    const char *origin_route = NULL;
    // uncompress route dictionary
    if(raw_msg->compressRoute) {
      origin_route = pc__resolve_dictionary(client, raw_msg->route.route_code);
      if(origin_route == NULL) {
        fprintf(stderr, "Fail to uncompress route dictionary: %d.\n",
                raw_msg->route.route_code);
        goto error;
      }
    } else {
      origin_route = raw_msg->route.route_str;
    }

    route_str = malloc(strlen(origin_route) + 1);
    if(route_str == NULL) {
      fprintf(stderr, "Fail to malloc for uncompress route dictionary.\n");
      goto error;
    }

    memcpy((void *)route_str, (void *)origin_route, strlen(origin_route) + 1);
    msg->route = route_str;
  } else {
    // must be response, then get the route from requests map
    char id_str[64];
    memset(id_str, 0, 64);
    sprintf(id_str, "%u", msg->id);
    pc_request_t *req = (pc_request_t *)pc_map_get(client->requests, id_str);
    if(req == NULL) {
      fprintf(stderr, "Fail to get correlative request for the response: %u\n",
              msg->id);
      goto error;
    }
    route_str = req->route;
  }

  pc_buf_t body = raw_msg->body;
  if(body.len > 0) {
    json_t *pb_def = json_object_get(client->server_protos, route_str);
    if(pb_def) {
      // protobuf decode
      msg->msg = pc__pb_decode(body.base, 0, body.len, pb_def);
    } else {
      // json decode
      msg->msg = pc__json_decode(body.base, 0, body.len);
    }

    if(msg->msg == NULL) {
      goto error;
    }
  }

  return msg;

error:
  if(msg == NULL && route_str) free((void *)route_str);
  if(raw_msg) pc__raw_msg_destroy(raw_msg);
  if(msg) pc_msg_destroy(msg);
  return NULL;
}

void pc__default_msg_parse_done_cb(pc_client_t *client, pc_msg_t *msg) {
  if(msg != NULL) {
    pc_msg_destroy(msg);
  }
}

pc_buf_t pc__default_msg_encode_cb(pc_client_t *client, uint32_t reqId,
                                   const char *route, json_t *msg) {
  pc_buf_t msg_buf, body_buf;
  msg_buf.len = -1;
  body_buf.len = -1;

  // route encode
  int route_code = 0;
  json_t *code = json_object_get(client->route_to_code, route);
  if(code) {
    // dictionary compress
    route_code = json_integer_value(code);
  }

  // encode body
  json_t *pb_def = json_object_get(client->client_protos, route);
  if(pb_def) {
    body_buf = pc__pb_encode(msg, pb_def);
    if(body_buf.len == -1) {
      fprintf(stderr, "Fail to encode message with protobuf: %s\n", route);
      goto error;
    }
  } else {
    // json encode
    body_buf = pc__json_encode(msg);
    if(body_buf.len == -1) {
      fprintf(stderr, "Fail to encode message with json: %s\n", route);
      goto error;
    }
  }

  // message type
  pc_msg_type type = PC_MSG_REQUEST;
  if(reqId == 0) {
    type = PC_MSG_NOTIFY;
  }

  if(route_code > 0) {
    msg_buf = pc_msg_encode_code(reqId, type, route_code, body_buf);
    if(msg_buf.len == -1) {
      fprintf(stderr, "Fail to encode message with route code: %d\n",
              route_code);
      goto error;
    }
  } else {
    msg_buf = pc_msg_encode_route(reqId, type, route, body_buf);
    if(msg_buf.len == -1) {
      fprintf(stderr, "Fail to encode message with route string: %s\n",
              route);
      goto error;
    }
  }

  if(body_buf.len > 0) {
    free(body_buf.base);
  }

  return msg_buf;

error:
  if(msg_buf.len > 0) free(msg_buf.base);
  if(body_buf.len > 0) free(body_buf.base);
  msg_buf.len = -1;
  return msg_buf;
}

void pc__default_msg_encode_done_cb(pc_client_t *client, pc_buf_t buf) {
  if(buf.len > 0) {
    free(buf.base);
  }
}

void pc__release_listeners(pc_map_t *map, const char* key, void *value) {
  if(value == NULL) {
    return;
  }

  ngx_queue_t *head = (ngx_queue_t *)value;
  ngx_queue_t *q;
  pc_listener_t *l;
  while (!ngx_queue_empty(head)) {
    q = ngx_queue_head(head);
    ngx_queue_remove(q);
    ngx_queue_init(q);

    l = ngx_queue_data(q, pc_listener_t, queue);
    pc_listener_destroy(l);
  }

  free(head);
}

void pc__release_requests(pc_map_t *map, const char* key, void *value) {
  if(value == NULL) {
    return;
  }

  pc_request_t *req = (pc_request_t *)value;
  req->cb(req, -1, NULL);
}

void pc__close_async_cb(uv_async_t *handle, int status) {
  pc_client_t *client = (pc_client_t *)handle->data;
  pc_client_stop(client);
  pc__client_clear(client);
}

void pc__cond_wait(pc_client_t *client, uint64_t timeout) {
  uv_mutex_lock(&client->mutex);
  if(timeout > 0) {
    uv_cond_timedwait(&client->cond, &client->mutex, 3000);
  } else {
    uv_cond_wait(&client->cond, &client->mutex);
  }
  uv_mutex_unlock(&client->mutex);
}

void pc__cond_broadcast(pc_client_t *client) {
  uv_mutex_lock(&client->mutex);
  uv_cond_broadcast(&client->cond);
  uv_mutex_unlock(&client->mutex);
}

void pc__client_connected_cb(pc_connect_t* req, int status) {
  if(status == -1) {
    pc_client_stop(req->client);
  }

  pc__cond_broadcast(req->client);
}

void pc__worker(void *arg) {
  pc_client_t *client = (pc_client_t *)arg;

  pc_run(client);

  // make sure the state
  client->state = PC_ST_CLOSED;

  pc_emit_event(client, PC_EVENT_DISCONNECT, NULL);
  pc__cond_broadcast(client);
}
