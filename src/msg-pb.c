#include <jansson.h>
#include <pomelo-protocol/message.h>

pc_buf_t pc__pb_encode(const char *route, const json_t *msg, json_t *pb) {
  pc_buf_t buf;
  return buf;
}

json_t *pc__pb_decode(const char *data, size_t offset, size_t len, json_t *pb) {
  return NULL;
}

json_t *pc__pb_get(const json_t *pb_map, const char *route) {
  return NULL;
}
