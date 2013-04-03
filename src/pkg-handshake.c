#include <string.h>
#include "pomelo.h"
#include "pomelo-protocol/package.h"

extern int pc__binary_write(pc_client_t *client, const char *data, size_t len,
                     uv_write_cb cb);

static int pc__handshake_ack(pc_client_t *client);
static void pc__handshake_req_cb(uv_write_t* req, int status);
static void pc__handshake_ack_cb(uv_write_t* req, int status);

int pc__handshake_req(pc_client_t *client) {
  json_t *handshake_opts = client->handshake_opts;
  json_t *body = json_object();
  const char *data = NULL;

  if(body == NULL) {
    fprintf(stderr, "Fail to create json_t for handshake request.\n");
    return -1;
  }

  // compose handshake package
  json_t *sys = json_object();
  if(sys == NULL) {
    fprintf(stderr, "Fail to create json_t for handshake request.\n");
    goto error;
  }

  json_object_set(body, "sys", sys);
  json_decref(sys);

  json_t *json_type = json_string(PC_TYPE);
  json_t *json_version = json_string(PC_VERSION);
  json_object_set(sys, "type", json_type);
  json_object_set(sys, "version", json_version);
  json_decref(json_type);
  json_decref(json_version);

  if(handshake_opts) {
    json_t *user = json_object_get(handshake_opts, "user");
    if(user) {
      json_object_set(body, "user", user);
    }
  }

  data = json_dumps(body, JSON_COMPACT);
  if(data == NULL) {
    fprintf(stderr, "Fail to compose Pomelo handshake package.\n");
    goto error;
  }

  pc_buf_t buf = pc_pkg_encode(PC_PKG_HANDSHAKE, data, strlen(data));
  if(pc__binary_write(client, buf.base, buf.len, pc__handshake_req_cb)) {
    fprintf(stderr, "Fail to send handshake request.\n");
    goto error;
  }

  free((void *)data);
  json_decref(body);

  return 0;
error:
  if(data) free((void *)data);
  if(body) json_decref(body);
  return -1;
}

int pc__handshake_resp(pc_client_t *client,
                       const char *data, size_t len) {
  json_error_t error;
  json_t *res = json_loadb(data, len, 0, &error);

  if(res == NULL) {
    fprintf(stderr, "Fail to parse handshake package. %s\n", error.text);
    goto error;
  }

  json_int_t code = json_integer_value(json_object_get(res, "code"));
  if(code != PC_HANDSHAKE_OK) {
    fprintf(stderr, "Handshake fail, code: %d.\n", (int)code);
    goto error;
  }

  json_t *sys = json_object_get(res, "sys");
  if(sys) {
    // setup heartbeat
    json_int_t hb = json_integer_value(json_object_get(sys, "heartbeat"));
    if(hb <= 0) {
      // no need heartbeat
      client->heartbeat = -1;
      client->timeout = -1;
    } else {
        client->heartbeat = hb * 1000;
      client->timeout = client->heartbeat * PC_HEARTBEAT_TIMEOUT_FACTOR;
      uv_timer_set_repeat(client->heartbeat_timer, client->heartbeat);
      uv_timer_set_repeat(client->timeout_timer, client->timeout);
    }

    // setup route dictionary
    json_t *dict = json_object_get(sys, "dict");
    if(dict) {
      client->route_to_code = dict;
      json_incref(dict);
      client->code_to_route = json_object();
      const char *key;
      json_t *value;
      char code_str[16];
      json_object_foreach(dict, key, value) {
        memset(code_str, 0, 16);
        sprintf(code_str, "%u", ((int)json_integer_value(value) & 0xff));
        json_t *json_key = json_string(key);
        json_object_set(client->code_to_route, code_str, json_key);
        json_decref(json_key);
      }
    }

    // setup protobuf data definition
    json_t *protos = json_object_get(sys, "protos");

    if(protos) {
      client->server_protos = json_object_get(protos, "server");
      client->client_protos = json_object_get(protos, "client");
      json_incref(client->server_protos);
      json_incref(client->client_protos);
    }
  }

  json_t *user = json_object_get(res, "user");
  int status = 0;
  if(client->handshake_cb) {
    status = client->handshake_cb(client, user);
  }

  // release json parse result
  json_decref(res);

  if(!status) {
    pc__handshake_ack(client);
  }

  return 0;

error:
  json_decref(res);
  return -1;
}

static int pc__handshake_ack(pc_client_t *client) {
  pc_buf_t buf = pc_pkg_encode(PC_PKG_HANDSHAKE_ACK, NULL, 0);
  if(buf.len == -1) {
    fprintf(stderr, "Fail to encode handshake ack package.\n");
    goto error;
  }

  if(pc__binary_write(client, buf.base, buf.len, pc__handshake_ack_cb)) {
    fprintf(stderr, "Fail to send handshake ack.\n");
    goto error;
  }

  return 0;

error:
  if(buf.len != -1) free(buf.base);
  return -1;
}

static void pc__handshake_req_cb(uv_write_t* req, int status) {
  void **data = (void **)req->data;
  pc_transport_t *transport = (pc_transport_t *)data[0];
  pc_client_t *client = transport->client;
  char *base = (char *)data[1];

  free(base);
  free(data);
  free(req);

  if(PC_TP_ST_WORKING != transport->state) {
    fprintf(stderr, "Invalid transport state for handshake req cb: %d.\n",
            transport->state);
    return;
  }

  if(status == -1) {
    fprintf(stderr, "Fail to write handshake request async, %s.\n",
            uv_err_name(uv_last_error(client->uv_loop)));
    pc_client_stop(client);
    if(client->conn_req) {
      pc_connect_t *conn_req = client->conn_req;
      client->conn_req = NULL;
      conn_req->cb(conn_req, status);
    }
  }
}

static void pc__handshake_ack_cb(uv_write_t* req, int status) {
  void **data = (void **)req->data;
  pc_transport_t *transport = (pc_transport_t *)data[0];
  pc_client_t *client = transport->client;
  char *base = (char *)data[1];

  free(base);
  free(data);
  free(req);

  if(PC_TP_ST_WORKING != transport->state) {
    fprintf(stderr, "Invalid transport state for handshake ack cb: %d.\n",
            transport->state);
    return;
  }

  if(status == -1) {
    fprintf(stderr, "Fail to write handshake ack async, %s.\n",
            uv_err_name(uv_last_error(client->uv_loop)));
    pc_client_stop(client);
  } else {
    client->state = PC_ST_WORKING;
  }

  if(client->conn_req) {
    pc_connect_t *conn_req = client->conn_req;
    client->conn_req = NULL;
    conn_req->cb(conn_req, status);
  }
}
