#include <assert.h>
#include <pomelo-protocol/message.h>

size_t pc__msg_decode_compressed_head(const char *data, size_t offset,
                                      size_t len, const json_t *code_dic,
                                      const char **route);
size_t pc__msg_decode_uncompressed_head(const char *data, size_t offset,
                                  size_t len, const char **route);

pc_buf_t pc_msg_encode(uint32_t id, const char *route, const json_t *msg,
                       json_t *route_dic, const json_t *pb_map) {
  pc_buf_t buf, body_buf;

  memset(&buf, 0, sizeof(pc_buf_t));
  memset(&body_buf, 0, sizeof(pc_buf_t));

  size_t total_len = PC_MSG_HEAD_BYTES;
  uint16_t code = json_integer_value(json_object_get(route_dic, route));
  int route_len = strlen(route) & 0xffff;   // route length is 1 byte

  if(code == 0) {
    // uncompressed head
    total_len += PC_MSG_UNCOMPRESSED_ROUTE_LEN_BYTES + route_len;
  } else {
    // compressed head
    total_len += PC_MSG_COMPRESSED_ROUTE_BYTES;
  }

  // encode body
  if(pb_map) {
    json_t *pb = pc__pb_get(pb_map, route);
    if(pb != NULL) {
      body_buf = pc__pb_encode(route, msg, pb);
    } else {
      body_buf = pc__json_encode(msg);
    }
  } else {
    body_buf = pc__json_encode(msg);
  }

  if(body_buf.len == -1) {
    goto error;
  }

  total_len += body_buf.len;

  buf.base = malloc(total_len);

  if(buf.base == NULL) {
    goto error;
  }

  buf.len = total_len;
  char *data = buf.base;

  // encode messge id
  uint32_t t_id = id;
  for(int i=PC_MSG_HEAD_BYTES-2; i>=0; i--) {
    data[i] = t_id & 0xff;
    t_id >>= 8;
  }

  size_t offset = PC_MSG_HEAD_BYTES - 1;
  if(code == 0) {
    // flags
    data[offset++] = 0;
    // route length
    data[offset++] = route_len;
    memcpy(data + offset, route, route_len);
    offset += route_len;
  } else {
    // flags
    data[offset++] = 1;
    // route code
    int t_code = code;
    char *t_base = data + offset;
    for(int i=PC_MSG_COMPRESSED_ROUTE_BYTES-1; i>=0; i++) {
      t_base[i] = t_code & 0xff;
      t_code >>= 8;
    }
    offset += PC_MSG_COMPRESSED_ROUTE_BYTES;
  }

  memcpy(data + offset, body_buf.base, body_buf.len);

  assert(body_buf.len > 0);
  free(body_buf.base);

  return buf;

error:
  if(buf.len != -1) free(buf.base);
  if(body_buf.len != -1) free(body_buf.base);
  buf.len = -1;
  return buf;
}

pc_msg_t *pc_msg_decode(const char *data, size_t len,
                        const json_t *code_dic, const json_t *pb_map) {
  if(len < PC_MSG_HEAD_BYTES) {
    fprintf(stderr, "Invalid Pomelo message.\n");
    return NULL;
  }

  pc_msg_t *msg = NULL;
  const char *route_str = NULL;

  msg = (pc_msg_t *)malloc(sizeof(pc_msg_t));
  if(msg == NULL) {
    fprintf(stderr, "Fail to malloc for pc_msg_t.\n");
    return NULL;
  }

  // parse message id
  uint32_t id = 0;
  for(int i=0; i<PC_MSG_HEAD_BYTES-1; i++) {
    if(i > 0) {
      id <<= 8;
    }
    id |= *(data + i);
  }

  uint8_t flags = *(data + PC_MSG_HEAD_BYTES);
  size_t offset = PC_MSG_HEAD_BYTES;

  // parse route string
  if(PC_MSG_IS_COMPRESSED_ROUTE(flags)) {
    offset = pc__msg_decode_compressed_head(data, offset, len,
                                            code_dic, &route_str);
  } else {
    offset = pc__msg_decode_uncompressed_head(data, offset, len, &route_str);
  }

  if(offset == -1) {
    goto error;
  }

  json_t *body = NULL;
  if(pb_map) {
    json_t *pb = pc__pb_get(pb_map, route_str);
    if(pb != NULL) {
      body = pc__pb_decode(data, offset, len, pb);
    } else {
      body = pc__json_decode(data, offset, len);
    }
  } else {
    body = pc__json_decode(data, offset, len);
  }

  if(body == NULL) {
    goto error;
  }

  msg->id = id;
  msg->route = route_str;
  msg->msg = body;

  return msg;

error:
  if(msg) free(msg);
  if(route_str) free((void *)route_str);
  return NULL;
}

void pc_msg_destroy(pc_msg_t *msg) {
  if(msg == NULL) return;
  if(msg->route) free((void *)msg->route);
  if(msg->msg) {
    json_decref(msg->msg);
    msg->msg = NULL;
  }
  free(msg);
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
size_t pc__msg_decode_compressed_head(const char *data, size_t offset,
                                      size_t len, const json_t *code_dic,
                                      const char **route) {
  uint16_t route_code = 0;
  const char *base = data + offset;
  for(int i=0; i<PC_MSG_COMPRESSED_ROUTE_BYTES; i++) {
    if(i > 0) {
      route_code <<= 8;
    }
    route_code |= base[i];
  }

  char code_str[32];
  memset(code_str, 0, 32);
  sprintf(code_str, "%u", route_code);
  *route = json_string_value(json_object_get(code_dic, code_str));

  if(*route == NULL) {
    return -1;
  }

  return offset + PC_MSG_COMPRESSED_ROUTE_BYTES;
}

size_t pc__msg_encode_compressed_head(uint16_t code) {
  return 0;
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
size_t pc__msg_decode_uncompressed_head(const char *data, size_t offset,
                                            size_t len, const char **route) {
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
