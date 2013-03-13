#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <pomelo-client.h>
#include <unistd.h>
#include <jansson.h>

const char *ip = "127.0.0.1";
int port = 3010;

// request callback
void on_request_cb(pc_request_t *req, int status, json_t *resp) {
  if(status == -1) {
    printf("Fail to send request to server.\n");
  } else if(status == 0) {
    printf("server response: %s\n", json_dumps(resp, 0));
  }

  // release relative resource with pc_request_t
  json_t *msg = req->msg;
  json_decref(msg);
  pc_request_destroy(req);

  pc_disconnect(req->client, 0);
}

void do_request(pc_client_t *client) {
  const char *route = "connector.helloHandler.hi";
  json_t *msg = json_object();
  json_object_set(msg, "msg", json_string("hi~"));

  pc_request_t *request = pc_request_new();
  pc_request(client, request, route, msg, on_request_cb);
}


// notified callback
void on_notified(pc_notify_t *req, int status) {
  if(status == -1) {
    printf("Fail to send notify to server.\n");
  } else {
    printf("Notify finished.\n");
  }

  // release resources
  json_t *msg = req->msg;
  json_decref(msg);
  pc_notify_destroy(req);
}

void do_notify(pc_client_t *client) {
  const char *route = "connector.helloHandler.hello";
  json_t *msg = json_object();
  json_object_set(msg, "msg", json_string("hello"));

  pc_notify_t *notify = pc_notify_new();
  pc_notify(client, notify, route, msg, on_notified);
}

// connection established callback
void on_connected(pc_connect_t* req, int status) {
  if(status == -1) {
    fprintf(stderr, "Fail to connect to server\n");
  } else {
    printf("Connection established.\n");
  }

  pc_client_t *client = req->client;

  // clean up
  free(req->address);
  free(req);

  // send notify
  do_notify(client);
}

void on_hey(pc_client_t *client, const char *event, void *data) {
  json_t *push_msg = (json_t *)data;
  printf("on event: %s, serve push msg: %s\n", event, json_dumps(push_msg, 0));

  // send request
  do_request(client);
}

int main() {
  // create and init Pomelo client instance
  pc_client_t *client = pc_client_new();

  if(client == NULL) {
    fprintf(stderr, "Fail to malloc for pc_client_t.\n");
    goto error;
  }

  struct sockaddr_in *address =
      (struct sockaddr_in *)malloc(sizeof (struct sockaddr_in));

  if(address == NULL) {
    fprintf(stderr, "Fail to malloc for address.\n");
    goto error;
  }

  memset(address, 0, sizeof(struct sockaddr_in));
  address->sin_family = AF_INET;
  address->sin_port = htons(port);
  address->sin_addr.s_addr = inet_addr(ip);

  pc_add_listener(client, "onHey", on_hey);

  // create connect request and connect to server
  pc_connect_t *conn_req = pc_connect_req_new(address);

  if(conn_req == NULL) {
    fprintf(stderr, "Fail to malloc pc_connect_t.\n");
    goto error;
  }

  if(pc_connect(client, conn_req, NULL, on_connected)) {
    fprintf(stderr, "Fail to connect to server.\n");
    goto error;
  }

  pc_run(client);

  pc_client_destroy(client);

  return 0;

error:
  if(!client) free(client);
  if(!address) free(address);
  if(!conn_req) free(conn_req);
  return -1;
}