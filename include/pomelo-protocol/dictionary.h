#ifndef PC_DICTIONARY_H
#define PC_DICTIONARY_H

#include <jansson.h>

const char *pc__msg_dic_route(json_t *dic, int code);

int pc__msg_dic_code(json_t *dic, const char *route);

#endif /* PC_DICTIONARY_H */