#include <assert.h>
#include <pomelo-protocol/message.h>

int main() {
  uint32_t id = 1;
  char *route = "connector.helloHandler.hello";
  json_t *msg = json_object();
  json_object_set(msg, "msg", json_string("hello"));

  pc_buf_t body_buf = pc__json_encode(msg);

  pc_buf_t buf = pc_msg_encode_route(id, PC_MSG_REQUEST, route, body_buf);

  assert(buf.len > 0);

  pc__msg_raw_t *raw_msg = pc_msg_decode(buf.base, buf.len);

  assert(raw_msg);
  assert(raw_msg->id == id);
  assert(!strcmp(route, raw_msg->route.route_str));
  assert(!memcmp(body_buf.base, raw_msg->body.base, body_buf.len));

  json_t *body = pc__json_decode(raw_msg->body.base, 0, raw_msg->body.len);

  assert(body);

  printf("after decode id: %d, route: %s, body: %s\n", raw_msg->id,
         raw_msg->route.route_str, json_dumps(body, 0));
}