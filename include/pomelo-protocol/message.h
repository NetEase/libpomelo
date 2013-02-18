#ifndef PC_MESSAGE_H
#define PC_MESSAGE_H

#include <jansson.h>
#include <pomelo-client.h>

/**
 * Pomelo message format:
 *
 * Format One: compressed route
 * +------+-------+------------+------------+
 * |  id  | flags | route code |    body    |
 * +------+-------+------------+------------+
 *     4      1          2
 *
 * Format Two: uncompressed route
 * +------+-------+--------------+--------------+-----------+
 * |  id  | flags | route lenght | route string |   body    |
 * +------+-------+--------------+--------------+-----------+
 *     4      1          1         route length
 *
 * Head: 5 bytes
 *   0 - 3: big-endian message id
 *   5: flags byte, lowest bit: 0 for uncompressed and 1 for compressed route
 * Route: variable
 *   uncompressed
 *     6 - 7: big-endian route code
 *   compressed
 *     6: route length
 *     7 - (route length - 1): utf8 encoded route string
 * Body: all the left bytes in package
 */

#define PC_MSG_HEAD_BYTES 5

#define PC_MSG_COMPRESSED_ROUTE_BYTES 2

#define PC_MSG_UNCOMPRESSED_ROUTE_LEN_BYTES 1

#define PC_MSG_IS_COMPRESSED_ROUTE(flags) (flags & 0x01)

typedef struct pc_msg_s pc_msg_t;

struct pc_msg_s {
  uint32_t id;
  const char *route;
  json_t *msg;
};

pc_buf_t pc_msg_encode(uint32_t id, const char *route, const json_t *msg,
                       json_t *route_dic, const json_t *pb_map);

pc_msg_t *pc_msg_decode(const char *data, size_t len,
                        const json_t *code_dic, const json_t *pb_map);

void pc_msg_destroy(pc_msg_t *msg);

pc_buf_t pc__json_encode(const json_t *msg);

json_t *pc__json_decode(const char *data, size_t offset, size_t len);

pc_buf_t pc__pb_encode(const char *route, const json_t *msg, json_t *pb);

json_t *pc__pb_decode(const char *data, size_t offset, size_t len, json_t *pb);

json_t *pc__pb_get(const json_t *pb_map, const char *route);

#endif /* PC_MESSAGE_H */