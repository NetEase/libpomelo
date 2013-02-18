#include <pomelo-client.h>

int pc__binary_write(pc_client_t *client, const char *data, size_t len,
                            uv_write_cb cb);
int pc__handshake_req(pc_client_t *client);
int pc__handshake_ack(pc_client_t *client);
int pc__handshake_resp(pc_client_t *client,
                                const char *data, size_t len);

static void pc__handshake_req_cb(uv_write_t* req, int status);
static void pc__handshake_ack_cb(uv_write_t* req, int status);

int pc__handshake_req(pc_client_t *client) {
  json_t *handshake_opts = client->handshake_opts;
  json_t *body = json_object();

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

  json_object_set(sys, "version", json_string(PC_VERSION));

  if(handshake_opts) {
    json_t *heartbeat = json_object_get(handshake_opts, "heartbeat");
    if(heartbeat) {
      json_object_set(sys, "heartbeat", heartbeat);
    }

    json_t *user = json_object_get(handshake_opts, "user");
    if(heartbeat) {
      json_object_set(body, "user", user);
    }
  }

  const char *data = json_dumps(body, JSON_COMPACT);
  if(data == NULL) {
    fprintf(stderr, "Fail to compose Pomelo handshake package.\n");
    goto error;
  }

  pc_buf_t buf = pc_pkg_encode(PC_PKG_HANDSHAKE, data, strlen(data));
  if(pc__binary_write(client, buf.base, buf.len, pc__handshake_req_cb)) {
    fprintf(stderr, "Fail to send handshake request.\n");
    goto error;
  }

  json_decref(body);

  return 0;
error:
  if(body) json_decref(body);
  return -1;
}

int pc__handshake_resp(pc_client_t *client,
                                const char *data, size_t len) {
  json_error_t error;
  json_t *res = json_loadb(data, len, 0, &error);

  if(res == NULL) {
    fprintf(stderr, "Fail to parse handshake package. %s\n", error.text);
    return -1;
  }

  json_int_t code = json_integer_value(json_object_get(res, "code"));
  if(code != PC_HANDSHAKE_OK) {
    fprintf(stderr, "Handshake fail, code: %d.\n", (int)code);
    return -1;
  }

  json_t *sys = json_object_get(res, "sys");
  if(sys) {
    // setup heartbeat
    json_int_t hb = json_integer_value(json_object_get(sys, "heartbeat"));
    if(hb < 0) {
      // no need heartbeat
      client->heartbeat = -1;
      client->timeout = -1;
    } else {
      client->heartbeat = hb > 0 ? hb : PC_DEFAULT_HEARTBEAT;
      json_int_t to = json_integer_value(json_object_get(sys, "timeout"));
      client->timeout = to > client->heartbeat ? to : PC_DEFAULT_TIMEOUT;
      uv_timer_set_repeat(&client->heartbeat_timer, client->heartbeat);
      uv_timer_set_repeat(&client->timeout_timer, client->timeout);
    }

    // setup route dictionary
    json_t *dics = json_object_get(sys, "dictionary");
    if(dics) {
      //TODO: dictionary name?
      client->route_to_code = NULL;
      client->code_to_route = NULL;
    }
  }

  json_t *user = json_object_get(res, "user");
  int status = 0;
  if(client->handshake_cb) {
    status = client->handshake_cb(client, user);
  }

  if(!status) {
    pc__handshake_ack(client);
  }

  return 0;
}

int pc__handshake_ack(pc_client_t *client) {
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
  pc_client_t *client = (pc_client_t *)data[0];
  char *base = (char *)data[1];

  free(base);
  free(data);
  free(req);

  if(status == -1) {
    fprintf(stderr, "Fail to write handshake request async, %s.\n",
            uv_err_name(uv_last_error(client->uv_loop)));
    pc_disconnect(client);
    if(client->conn_req) {
      client->conn_req->cb(client->conn_req, status);
      client->conn_req = NULL;
    }
  }
}

static void pc__handshake_ack_cb(uv_write_t* req, int status) {
  void **data = (void **)req->data;
  pc_client_t *client = (pc_client_t *)data[0];
  char *base = (char *)data[1];

  free(base);
  free(data);
  free(req);

  if(status == -1) {
    fprintf(stderr, "Fail to write handshake ack async, %s.\n",
            uv_err_name(uv_last_error(client->uv_loop)));
    pc_disconnect(client);
  } else {
    client->state = PC_ST_WORKING;
    if(client->conn_req) {
      client->conn_req->cb(client->conn_req, 0);
      client->conn_req = NULL;
    }
  }
}

static int pc__handshake_sys(pc_client_t *client, json_t *sys) {
  json_int_t hb = json_integer_value(json_object_get(sys, "heartbeat"));
  if(hb < 0) {
    // no need heartbeat
    client->heartbeat = -1;
    client->timeout = -1;
  } else {
    client->heartbeat = hb > 0 ? hb : PC_DEFAULT_HEARTBEAT;
    json_int_t to = json_integer_value(json_object_get(sys, "timeout"));
    client->timeout = to > client->heartbeat ? to : PC_DEFAULT_TIMEOUT;
  }

  uv_timer_set_repeat(&client->heartbeat_timer, client->heartbeat);
  uv_timer_set_repeat(&client->timeout_timer, client->timeout);

  return 0;
}