#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "pomelo.h"

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
}

void do_request(pc_client_t *client) {
  // compose request
  const char *route = "connector.helloHandler.hi";
  json_t *msg = json_object();
  json_object_set(msg, "msg", json_string("hi~"));

  pc_request_t *request = pc_request_new();
  pc_request(client, request, route, msg, on_request_cb);
}

// disconnect event callback.
void on_close(pc_client_t *client, const char *event, void *data) {
  printf("client closed: %d.\n", client->state);
}


int main() {
  // create a client instance.
  pc_client_t *client = pc_client_new();

  struct sockaddr_in address;

  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = inet_addr(ip);

  // add some event callback.
  pc_add_listener(client, PC_EVENT_DISCONNECT, on_close);

  // try to connect to server.
  if(pc_client_connect(client, &address)) {
    printf("fail to connect server.\n");
    pc_client_destroy(client);
    return 1;
  }

  do_request(client);

  // main thread sleep 3 second and then destroy the client instance.
  printf("Main thread sleep 3 second and then destroy the client instance.\n");
  sleep(3);

  // release the client
  pc_client_destroy(client);

  return 0;
}
