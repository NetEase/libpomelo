#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "pomelo.h"
#include "pomelo-private/listener.h"
#include "pomelo-protocol/package.h"
#include "pomelo-protocol/message.h"
#include "pomelo-private/transport.h"
#include "pomelo-private/internal.h"
#include "pomelo-private/common.h"
#include "pomelo-private/ngx-queue.h"

static void pc__client_init(pc_client_t *client);
static void pc__close_async_cb(uv_async_t *handle, int status);
static void pc__release_listeners(pc_map_t *map, const char* key, void *value);
static void pc__release_requests(pc_map_t *map, const char* key, void *value);

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
  client->listeners = pc_map_new(256, pc__release_listeners);
  if(client->listeners == NULL) {
    fprintf(stderr, "Fail to init client->listeners.\n");
    abort();
  }

  client->requests = pc_map_new(256, pc__release_requests);
  if(client->requests == NULL) {
    fprintf(stderr, "Fail to init client->requests.\n");
    abort();
  }

  client->pkg_parser = pc_pkg_parser_new(pc__pkg_cb, client);
  if(client->pkg_parser == NULL) {
    fprintf(stderr, "Fail to init client->pkg_parser.\n");
    abort();
  }

  client->heartbeat_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  if(client->heartbeat_timer == NULL) {
    fprintf(stderr, "Fail to malloc client->heartbeat_timer.\n");
    abort();
  }
  if(uv_timer_init(client->uv_loop, client->heartbeat_timer)) {
    fprintf(stderr, "Fail to init client->heartbeat_timer.\n");
    abort();
  }
  client->heartbeat_timer->timer_cb = pc__heartbeat_cb;
  client->heartbeat_timer->data = client;
  client->heartbeat = 0;

  client->timeout_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  if(client->timeout_timer == NULL) {
    fprintf(stderr, "Fail to malloc client->timeout_timer.\n");
    abort();
  }
  if(uv_timer_init(client->uv_loop, client->timeout_timer)) {
    fprintf(stderr, "Fail to init client->timeout_timer.\n");
    abort();
  }
  client->timeout_timer->timer_cb = pc__timeout_cb;
  client->timeout_timer->data = client;
  client->timeout = 0;

  client->close_async = (uv_async_t *)malloc(sizeof(uv_async_t));
  uv_async_init(client->uv_loop, client->close_async, pc__close_async_cb);
  client->close_async->data = client;
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
  if(PC_ST_INITED == client->state) {
    client->state = PC_ST_CLOSED;
    return;
  }

  if(PC_ST_CLOSED == client->state) {
    return;
  }

  client->state = PC_ST_CLOSED;
  if(client->transport) {
    pc_transport_destroy(client->transport);
    client->transport = NULL;
  }

  if(client->heartbeat_timer != NULL) {
    uv_close((uv_handle_t *)client->heartbeat_timer, pc__handle_close_cb);
    client->heartbeat_timer = NULL;
    client->heartbeat = 0;
  }
  if(client->timeout_timer != NULL) {
    uv_close((uv_handle_t *)client->timeout_timer, pc__handle_close_cb);
    client->timeout_timer = NULL;
    client->timeout = 0;
  }
  if(client->close_async != NULL) {
    uv_close((uv_handle_t *)client->close_async, pc__handle_close_cb);
    client->close_async = NULL;
  }
}


void pc__client_force_join(pc_client_t *client){
  // If worker is never initialized, worker should be null
  // and pthread_join will rise segment faul
  if(client->worker){
    uv_thread_join(&client->worker);
  }
}

void pc_client_destroy(pc_client_t *client) {
  if(PC_ST_INITED == client->state) {
    //! pc__client_clear(client);
    goto finally;
  }

  if(PC_ST_CLOSED == client->state) {
    //! pc__client_clear(client);
    goto finally;
  }

  // 1. asyn worker thread
  // 2. wait work thread exit
  // 3. free client
  uv_async_send(client->close_async);

  pc_client_join(client);

  if(PC_ST_CLOSED != client->state) {
    pc_client_stop(client);
    // wait uv_loop_t stop
#ifdef _WIN32
	Sleep(1000);
#else
	sleep(1);
#endif

    //! pc__client_clear(client);
  }

finally:
  //~ Issue:
  //~ 1. The worker may clean up 'client', which leads to race
  //~ 2. The main thread is cleaning up 'client', while the worker is busy broadcasting its demise
  //~       after setting client state to PC_ST_CLOSED
  //~       (the worker thread iterates thru 'listeners', where the memory may corrupt.

  pc__client_force_join(client);
  pc__client_clear(client);

  if(client->uv_loop) {
    uv_loop_delete(client->uv_loop);
    client->uv_loop = NULL;
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

int pc_client_connect2(pc_client_t *client, pc_connect_t *conn_req, pc_connect_cb cb){
  if(pc_connect(client, conn_req, NULL, cb)){
    // When failed, user should be responsible of reclaiming conn_req's memory
	// When succeeded, user should be responsible of reclaiming con_req's memory in cb
	// This API do not hold any assumption of conn_req(How it resides in memory)
    fprintf(stderr,"Fail to connect server.\n");
	return -1;
  }
  
  uv_thread_create(&client->worker, pc__worker, client);
  return 0;
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

  uv_mutex_lock(&client->listener_mutex);
  ngx_queue_t *head = (ngx_queue_t *)pc_map_get(client->listeners, event);

  if(head == NULL) {
    head = (ngx_queue_t *)malloc(sizeof(ngx_queue_t));
    if(head == NULL) {
      fprintf(stderr, "Fail to create listener queue.\n");
      pc_listener_destroy(listener);
      uv_mutex_unlock(&client->listener_mutex);
      return -1;
    }

    ngx_queue_init(head);

    pc_map_set(client->listeners, event, head);
  }

  ngx_queue_insert_tail(head, &listener->queue);
  uv_mutex_unlock(&client->listener_mutex);

  return 0;
}

void pc_remove_listener(pc_client_t *client, const char *event, pc_event_cb cb) {
  uv_mutex_lock(&client->listener_mutex);
  ngx_queue_t *head = (ngx_queue_t *)pc_map_get(client->listeners, event);
  if(head == NULL) {
    uv_mutex_unlock(&client->listener_mutex);
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
  uv_mutex_unlock(&client->listener_mutex);
}

void pc_emit_event(pc_client_t *client, const char *event, void *data) {
  uv_mutex_lock(&client->listener_mutex);
  ngx_queue_t *head = (ngx_queue_t *)pc_map_get(client->listeners, event);
  if(head == NULL) {
    uv_mutex_unlock(&client->listener_mutex);
    return;
  }

  ngx_queue_t *item = NULL;
  pc_listener_t *listener = NULL;

  ngx_queue_foreach(item, head) {
    listener = ngx_queue_data(item, pc_listener_t, queue);
    listener->cb(client, event, data);
  }
  uv_mutex_unlock(&client->listener_mutex);
}

int pc_run(pc_client_t *client) {
  if(!client || !client->uv_loop) {
    fprintf(stderr, "Invalid client to run.\n");
    return -1;
  }
  return uv_run(client->uv_loop, UV_RUN_DEFAULT);
};

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
}
