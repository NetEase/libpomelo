#include <stdio.h>
#include <jansson.h>

const char *pc__msg_dic_route(json_t *dic, int code) {
  char code_str[8];
  sprintf(code_str, "%d", code);
  json_t *route = json_object_get(dic, code_str);
  return json_string_value(route);
}

int pc__msg_dic_code(json_t *dic, const char *route) {
  json_t * code = json_object_get(dic, route);
  return json_integer_value(code);
}