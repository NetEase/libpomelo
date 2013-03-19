#include "pomelo.h"
#include "pomelo-private/internal.h"

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