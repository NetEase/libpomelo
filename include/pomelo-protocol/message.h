#ifndef PC_MESSAGE_H
#define PC_MESSAGE_H

#include "jansson.h"
#include "pomelo.h"

#define PC_MSG_FLAG_BYTES 1

#define PC_MSG_ROUTE_LEN_BYTES 1

#define PC_MSG_ROUTE_CODE_BYTES 2

#define PC_MSG_HAS_ID(TYPE) ((TYPE) == PC_MSG_REQUEST ||                       \
                             (TYPE) == PC_MSG_RESPONSE)

#define PC_MSG_HAS_ROUTE(TYPE) ((TYPE) != PC_MSG_RESPONSE)

#define PC_MSG_VALIDATE(TYPE) ((TYPE) == PC_MSG_REQUEST ||                     \
                               (TYPE) == PC_MSG_NOTIFY ||                      \
                               (TYPE) == PC_MSG_RESPONSE ||                    \
                               (TYPE) == PC_MSG_PUSH)

#define PC__MSG_CHECK_LEN(INDEX, LENGTH) do {                                  \
  if((INDEX) > (LENGTH)) {                                                     \
    goto error;                                                                \
  }                                                                            \
}while(0);

#define PC_PB_EVAL_FACTOR 2

typedef enum {
  PC_MSG_REQUEST = 0,
  PC_MSG_NOTIFY,
  PC_MSG_RESPONSE,
  PC_MSG_PUSH
} pc_msg_type;

typedef struct {
  uint32_t id;
  pc_msg_type type;
  uint8_t compressRoute;
  union {
    uint16_t route_code;
    const char *route_str;
  } route;
  pc_buf_t body;
} pc__msg_raw_t;

pc_buf_t pc_msg_encode_route(uint32_t id, pc_msg_type type,
                             const char *route, pc_buf_t msg);

pc_buf_t pc_msg_encode_code(uint32_t id, pc_msg_type type,
                            int route_code, pc_buf_t msg);

pc__msg_raw_t *pc_msg_decode(const char *data, size_t len);

void pc__raw_msg_destroy(pc__msg_raw_t *msg);

void pc_msg_destroy(pc_msg_t *msg);

pc_buf_t pc__json_encode(const json_t *msg);

json_t *pc__json_decode(const char *data, size_t offset, size_t len);

/**
 * Do protobuf encode for message. The pc_buf_t returned contains the encode
 * result in buf.base and the size of the data in buf.len which should be
 * positive and should be -1 stand for error. And if success, the buf.base MUST
 * be released by free().
 *
 * @param  msg    json message to be encoded
 * @param  pb_def protobuf definition for the message
 * @return        encode result
 */
pc_buf_t pc__pb_encode(const json_t *msg, const json_t *pb);

/**
 * Do protobuf decode for the binary data. The return json_t object could be
 * used as normal json_t object and call json_decref() to released.
 *
 * @param data    binary data to decode
 * @param offset  offset of the data
 * @param len     lenght of the data
 * @param pb_def  protobuf definition for the data
 * @return        decode result
 */
json_t *pc__pb_decode(const char *data, size_t offset, size_t len,
                      const json_t *pb_def);

#endif /* PC_MESSAGE_H */