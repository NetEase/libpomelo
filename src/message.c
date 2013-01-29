#include <pomelo-protocol/message.h>

size_t pc__msg_compressed_head(const char *data, size_t offset,
                                  size_t len, char **route);
size_t pc__msg_uncompressed_head(const char *data, size_t offset,
                                  size_t len, char **route);

pc_buf_t pc_message_encode(const char *route, const json_t *msg) {
  pc_buf_t buf;
  return buf;
}

pc_msg_t *pc_message_decode(const char *data, size_t len) {
  if(len < PC_MSG_HEAD_BYTES) {
    fprintf(stderr, "Invalid Pomelo message.\n");
    return NULL;
  }

  pc_msg_t *msg = (pc_msg_t *)malloc(sizeof(pc_msg_t));
  if(msg == NULL) {
    fprintf(stderr, "Fail to malloc for pc_msg_t.\n");
    return NULL;
  }

  uint32_t id = 0;
  for(int i=0; i<PC_MSG_HEAD_BYTES; i++) {
    if(i > 0) {
      id <<= 8;
    }
    id |= *(data + i);
  }

  uint8_t flags = *(data + PC_MSG_HEAD_BYTES);
  size_t offset = PC_MSG_HEAD_BYTES + PC_MSG_COMPRESSED_ROUTE_BYTES;
  char *route_str;

  // parse route string
  if(PC_MSG_IS_COMPRESSED_ROUTE(flags)) {
    offset = pc__msg_compressed_head(data, offset, len, &route_str);
  } else {
    offset = pc__msg_uncompressed_head(data, offset, len, &route_str);
  }

  if(offset == -1) {
    goto error;
  }

  msg->id = id;
  msg->route = route_str;

  return msg;

error:
  if(msg) free(msg);
  if(route_str) free(route_str);
  return NULL;
}

void pc_message_destroy(pc_msg_t *msg) {

}

/**
 * Parse compressed route code.
 * Route string returned by route parameter and should be deallocated outside.
 *
 * @param  data   message data
 * @param  offset comopressed route code offset to start to parse
 * @param  len    message data buffer length
 * @param  route  route string if parse successfully or NULL for fail
 * @return        new offset for message data or -1 for error
 */
size_t pc__msg_compressed_head(const char *data, size_t offset,
                                  size_t len, char **route) {
  uint16_t route_code = 0;
  const char *base = data + offset;
  for(int i=0; i<PC_MSG_COMPRESSED_ROUTE_BYTES; i++) {
    if(i > 0) {
      route_code <<= 8;
    }
    route_code |= base[i];
  }

  printf("route code: %d\n", route_code);
  return offset + PC_MSG_COMPRESSED_ROUTE_BYTES;
}

/**
 * Parse uncompressed route string.
 *
 * @param  data   message data
 * @param  offset comopressed route code offset to start to parse
 * @param  len    message data buffer length
 * @param  route  route string if parse successfully or unspecified for fail
 * @return        new offset for message data or -1 for error
 */
size_t pc__msg_uncompressed_head(const char *data, size_t offset,
                                            size_t len, char **route) {
  const char *base = data + offset;
  size_t route_len = 0;

  for(int i=0; i<PC_MSG_UNCOMPRESSED_ROUTE_LEN_BYTES; i++, base++) {
    if(i > 0) {
      route_len <<= 8;
    }
    route_len |= (unsigned char)*base;
  }

  offset += PC_MSG_UNCOMPRESSED_ROUTE_LEN_BYTES + route_len;
  if(offset >= len) {
    return -1;
  }

  char *route_str = malloc(route_len + 1);

  if(route_str == NULL) {
    fprintf(stderr, "Fail to malloc for route string of message.\n");
    return -1;
  }

  memcpy(route_str, base, route_len);
  route_str[route_len] = '\0';

  if(route != NULL) {
    *route = route_str;
  }

  return offset;
}
