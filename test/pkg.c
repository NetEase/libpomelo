#include <assert.h>
#include <pomelo-protocol/package.h>

void on_pkg(pc_pkg_type type, const char *data,
              size_t len, void *attach) {
  char *msg = malloc(len + 1);
  memset(msg, 0, len + 1);
  memcpy(msg, data, len);
  printf("after encode type: %d, msg: %s\n", type, msg);
}

int main() {
  pc_pkg_parser_t parser;
  pc_pkg_parser_init(&parser, on_pkg, NULL);

  json_t *msg = json_object();
  json_object_set(msg, "msg", json_string("hello"));
  const char *msg_str = json_dumps(msg, 0);

  printf("before encode type: %d, msg: %s\n", PC_PKG_DATA, msg_str);

  pc_buf_t buf = pc_pkg_encode(PC_PKG_DATA, msg_str, strlen(msg_str));

  assert(buf.len > 0);

  pc_pkg_parser_feed(&parser, buf.base, buf.len);

  return 0;
}