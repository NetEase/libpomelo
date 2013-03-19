#include "pomelo.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

const char *ip = "127.0.0.1";
int port = 3010;

int count = 5;

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

  count--;
  if(count == 0)
    pc_client_stop(req->client);
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

void on_hey(pc_client_t *client, const char *event, void *data) {
  json_t *push_msg = (json_t *)data;
  const char *json_str = json_dumps(push_msg, 0);
  printf("on event: %s, serve push msg: %s\n", event, json_str);
  free((void *)json_str);
  do_request(client);
}

void on_close(pc_client_t *client, const char *event, void *data) {
  printf("client closed: %d.\n", client->state);
}

void do_notify(pc_client_t *client) {
  const char *route = "connector.helloHandler.hello";
  json_t *msg = json_object();
  json_object_set(msg, "msg", json_string("hello"));

  pc_notify_t *notify = pc_notify_new();
  pc_notify(client, notify, route, msg, on_notified);
}

int main() {
  pc_client_t *client = pc_client_new();

  struct sockaddr_in address;

  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = inet_addr(ip);

  pc_add_listener(client, "onHey", on_hey);
  pc_add_listener(client, PC_EVENT_DISCONNECT, on_close);

  if(pc_client_connect(client, &address)) {
    printf("fail to connect server.\n");
    goto error;
  }

  int i;
  for(i=0; i<count; i++) {
    do_notify(client);
  }

  pc_client_join(client);

  //sleep(3);

  printf("before main return.\n");

  pc_client_destroy(client);

  return 0;

error:
  pc_client_destroy(client);
  return 1;
}