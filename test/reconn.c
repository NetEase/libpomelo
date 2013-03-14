#include <pomelo-client.h>
#include <unistd.h>

const char *ip = "127.0.0.1";
int port = 3010;

void on_hey(pc_client_t *client, const char *event, void *data) {
  json_t *push_msg = (json_t *)data;
  printf("on event: %s, serve push msg: %s\n", event, json_dumps(push_msg, 0));
}

int main() {
  for(int i=0; i<3; i++) {
    pc_client_t *client = pc_client_new();

    struct sockaddr_in address;

    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip);

    pc_add_listener(client, "onHey", on_hey);

    if(pc_client_connect(client, &address)) {
      printf("fail to connect server: %d.\n", i);
      break;
    }

    printf("going to release: %d.\n", i);

    pc_client_destroy(client);
  }

  return 0;
}