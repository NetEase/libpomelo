#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <pomelo-client.h>
#include <pomelo-private/listener.h>

static uv_loop_t *global_uv_loop = NULL;

int pc__binary_write(pc_client_t *client, const char *data, size_t len,
                            uv_write_cb cb) ;
static void pc__pkg_cb(pc_pkg_type type, const char *data, size_t len,
                       void *attach);
int pc__handshake_resp(pc_client_t *client,
                                const char *data, size_t len);
int pc__heartbeat(pc_client_t *client);
int pc__heartbeat_req(pc_client_t *client);
void pc__heartbeat_cb(uv_timer_t* heartbeat_timer, int status);
void pc__timeout_cb(uv_timer_t* timeout_timer, int status);

int pc_client_init(pc_client_t* client) {
  if(!client) {
    fprintf(stderr, "Invalid pc_client_t to init.\n");
    return -1;
  }

  client->uv_loop = uv_loop_new();
  if(client->uv_loop == NULL) {
    fprintf(stderr, "Fail to create uv_loop_t.\n");
    abort();
  }

  if(pc_map_init(&client->listeners)) {
    fprintf(stderr, "Fail to init client->listeners.\n");
    abort();
  }

  if(pc_pkg_parser_init(&client->pkg_parser, pc__pkg_cb, client)) {
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

  client->state = PC_ST_INITED;

  return 0;
}

int pc_add_listener(pc_client_t *client, const char *event,
                    pc_event_cb event_cb) {
  if(client->state < PC_ST_INITED) {
    fprintf(stderr, "Pomelo client not init.\n");
    return -1;
  }

  pc_listener_t *listener = pc_listener_new();
  if(listener == NULL) {
    fprintf(stderr, "Fail to create listener.\n");
    return -1;
  }
  listener->cb = event_cb;

  ngx_queue_t *head = pc_map_get(&client->listeners, event);

  if(head == NULL) {
    head = (ngx_queue_t *)malloc(sizeof(ngx_queue_t));
    if(head == NULL) {
      fprintf(stderr, "Fail to create listener queue.\n");
      pc_listener_destroy(listener);
      return -1;
    }

    ngx_queue_init(head);

    pc_map_set(&client->listeners, event, head);
  }

  ngx_queue_insert_tail(head, &listener->queue);

  return 0;
}

void pc_remove_listener(pc_client_t *client, const char *event, pc_event_cb cb) {
  ngx_queue_t *head = (ngx_queue_t *)pc_map_get(&client->listeners, event);
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
    pc_map_del(&client->listeners, event);
    free(head);
  }
}

void pc_emit_event(pc_client_t *client, const char *event, void *attach) {
  ngx_queue_t *head = (ngx_queue_t *)pc_map_get(&client->listeners, event);
  if(head == NULL) {
    return;
  }

  ngx_queue_t *item = NULL;
  pc_listener_t *listener = NULL;

  ngx_queue_foreach(item, head) {
    listener = ngx_queue_data(item, pc_listener_t, queue);
    listener->cb(event, attach);
  }
};

int pc_disconnect(pc_client_t *client) {
  if(client->state == PC_ST_CLOSED) {
    return 0;
  }

  client->state = PC_ST_CLOSED;

  pc_emit_event(client, PC_EVENT_DISCONNECT, NULL);

  uv_close((uv_handle_t *)&client->socket, NULL);

  return 0;
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
    break;
    default:
      fprintf(stderr, "Unknown Pomelo package type: %d.\n", type);
    break;
  }

  if(status == -1) {
    pc_disconnect(client);
  }
}

int pc__data(pc_client_t *client, const char*data, size_t len) {
  return 0;
}