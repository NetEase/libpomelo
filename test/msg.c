#include <assert.h>
#include <pomelo-protocol/message.h>

int main() {
  char *route = "connector.helloHandler.hello";
  json_t *msg = json_object();
  json_object_set(msg, "msg", json_string("hello"));

  pc_buf_t buf = pc_msg_encode(1, route, msg, NULL, NULL);

  assert(buf.len > 0);

  for(int i=0; i<buf.len; i++) {
    printf("%02x ", buf.base[i]);
  }

  printf("\n");

  pc_msg_t *pmsg = pc_msg_decode(buf.base, buf.len, NULL, NULL);

  assert(pmsg);

  printf("after decode id: %d, route: %s, body: %s\n", pmsg->id, pmsg->route,
         json_dumps(pmsg->msg, 0));
}