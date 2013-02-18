#ifndef POMELO_PACKAGE_H
#define POMELO_PACKAGE_H

#include <uv.h>
#include <jansson.h>

/**
 * Pomelo package format:
 * +------+-------------+------------------+
 * | type | body length |       body       |
 * +------+-------------+------------------+
 *
 * Head: 4bytes
 *   0: package type, see as pc_pkg_type
 *   1 - 3: big-endian body length
 * Body: body length bytes
 */

#define PC_PKG_TYPE_MASK 0xff
#define PC_PKG_TYPE_BYTES 1
#define PC_PKG_BODY_LEN_BYTES 3
#define PC_PKG_HEAD_BYTES (PC_PKG_TYPE_BYTES + PC_PKG_BODY_LEN_BYTES)
#define PC_PKG_MAX_BODY_BYTES (1 << 24)

#define pc__pkg_type(head) (head[0] & 0xff)

typedef struct pc_pkg_parser_s pc_pkg_parser_t;
typedef struct pc_buf_s pc_buf_t;

typedef enum {
  PC_PKG_HEAD = 1,
  PC_PKG_BODY,
  PC_PKG_CLOSED
} pc_pkg_parser_state;

typedef enum {
  PC_PKG_HANDSHAKE = 1,
  PC_PKG_HANDSHAKE_ACK,
  PC_PKG_HEARBEAT,
  PC_PKG_DATA
} pc_pkg_type;

typedef void (*pc_pkg_cb)(pc_pkg_type type, const char *data,
              size_t len, void *attach);

struct pc_pkg_parser_s {
  char head_buf[PC_PKG_HEAD_BYTES];
  char *pkg_buf;

  size_t head_offset;
  size_t head_size;

  size_t pkg_offset;
  size_t pkg_size;

  pc_pkg_cb cb;
  void *attach;

  pc_pkg_parser_state state;
};

struct pc_buf_s {
  char *base;
  size_t len;
};

pc_pkg_parser_t *pc_pkg_parser_new(pc_pkg_cb cb, void *attach);

int pc_pkg_parser_init(pc_pkg_parser_t *pro, pc_pkg_cb cb, void *attach);

void pc_pkg_parser_close(pc_pkg_parser_t *parser);

void pc_pkg_parser_destroy(pc_pkg_parser_t *parser);

void pc_pkg_parser_destroy(pc_pkg_parser_t *pro);

void pc_pkg_parser_reset(pc_pkg_parser_t *pro);

int pc_pkg_parser_feed(pc_pkg_parser_t *pro, const char *data, size_t nread);

pc_buf_t pc_pkg_encode(pc_pkg_type type, const char *data, size_t len);

pc_buf_t pc_json_encode(const char *route, const json_t *msg);

#endif /* POMELO_PACKAGE_H */