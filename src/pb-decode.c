/* pb_decode.c -- decode a protobuf using minimal resources
 *
 * 2013 fantasyni<fantasyni@163.com>
 */

/* The warn_unused_result attribute appeared first in gcc-3.4.0 */
#if !defined(__GNUC__) || ( __GNUC__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
#define checkreturn
#else
/* Verify that we remember to check all return values for proper error propagation */
#define checkreturn __attribute__((warn_unused_result))
#endif

//#define __BIG_ENDIAN__
//#define PB_DEBUG
#define PB_INTERNALS
#include "pomelo-protobuf/pb.h"
#include "pomelo-protobuf/pb-util.h"
#include <string.h>
#include <stdlib.h>

typedef struct _pb_istream_t pb_istream_t;

/* Lightweight input stream.
 * You can provide a callback function for reading or use
 * pb_istream_from_buffer.
 *
 * Rules for callback:
 * 1) Return false on IO errors. This will cause decoding to abort.
 *
 * 2) You can use state to store your own data (e.g. buffer pointer),
 * and rely on pb_read to verify that no-body reads past bytes_left.
 *
 * 3) Your callback may be used with substreams, in which case bytes_left
 * is different than from the main stream. Don't use bytes_left to compute
 * any pointers.
 */
struct _pb_istream_t
{
  bool (*callback)(pb_istream_t *stream, uint8_t *buf, size_t count);
  void *state; /* Free field for use by callback implementation */
  size_t bytes_left;

#ifndef PB_NO_ERRMSG
  const char *errmsg;
#endif
};

static bool pb_decode(pb_istream_t *stream, json_t *protos, json_t *result);

static pb_istream_t pb_istream_from_buffer(uint8_t *buf, size_t bufsize);
static bool pb_read(pb_istream_t *stream, uint8_t *buf, size_t count);


/* --- Helper functions ---
 * You may want to use these from your caller or callbacks.
 */
static bool pb_decode_proto(pb_istream_t *stream, json_t *proto, json_t *protos, const char *key, json_t *result);

static bool pb_decode_array(pb_istream_t *stream, json_t *proto, json_t *protos, const char *key, json_t *result);

/* Decode the tag for the next field in the stream. Gives the wire type and
 * field tag. At end of the message, returns false and sets eof to true. */
static bool pb_decode_tag(pb_istream_t *stream, int *wire_type, uint32_t *tag, bool *eof);

/* Decode an integer in the varint format. This works for bool, enum, int32,
 * int64, uint32 and uint64 field types. */
static bool pb_decode_varint(pb_istream_t *stream, uint64_t *dest);

static bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest);

/* Decode an integer in the zig-zagged svarint format. This works for sint32
 * and sint64. */
static bool pb_decode_svarint(pb_istream_t *stream, int64_t *dest);

/* Decode a fixed32, sfixed32 or float value. You need to pass a pointer to
 * a 4-byte wide C variable. */
static bool pb_decode_fixed32(pb_istream_t *stream, void *dest);

/* Decode a fixed64, sfixed64 or double value. You need to pass a pointer to
 * a 8-byte wide C variable. */
static bool pb_decode_fixed64(pb_istream_t *stream, void *dest);

/* Make a limited-length substream for reading a PB_string field. */
static bool pb_make_string_substream(pb_istream_t *stream, pb_istream_t *substream);
static void pb_close_string_substream(pb_istream_t *stream, pb_istream_t *substream);

/* Decode a string */
static bool checkreturn pb_decode_strlen(pb_istream_t *stream, uint32_t *size);
static bool pb_decode_string(pb_istream_t *stream, void *dest, uint32_t size);

/* Decode submessage in __messages protos */
static bool pb_decode_submessage(pb_istream_t *stream, json_t *protos, void *dest);

bool pc_pb_decode(uint8_t *buf, size_t len, json_t *protos,
          json_t *result) {
  pb_istream_t stream = pb_istream_from_buffer(buf, len);
  if(!pb_decode(&stream, protos, result)) {
    fprintf(stderr, "decode error\n");
    return 0;
  }
  return 1;
}

/**************
 * pb_istream *
 **************/

static bool checkreturn buf_read(pb_istream_t *stream, uint8_t *buf,
    size_t count) {
  uint8_t *source = (uint8_t*) stream->state;

  if (buf != NULL )
    memcpy(buf, source, count);

  stream->state = source + count;
  return true;
}

static bool checkreturn pb_read(pb_istream_t *stream, uint8_t *buf, size_t count) {
  if (buf == NULL && stream->callback != buf_read) {
    /* Skip input bytes */
    uint8_t tmp[16];
    while (count > 16) {
      if (!pb_read(stream, tmp, 16))
        return false;

      count -= 16;
    }

    return pb_read(stream, tmp, count);
  }

  if (stream->bytes_left < count)
    PB_RETURN_ERROR(stream, "end-of-stream");

  if (!stream->callback(stream, buf, count))
    PB_RETURN_ERROR(stream, "io error");

  stream->bytes_left -= count;
  return true;
}

static pb_istream_t pb_istream_from_buffer(uint8_t *buf, size_t bufsize) {
  pb_istream_t stream;
  stream.callback = &buf_read;
  stream.state = buf;
  stream.bytes_left = bufsize;
#ifndef PB_NO_ERRMSG
  stream.errmsg = NULL;
#endif
  return stream;
}

/*************************
 * Decode a single field *
 *************************/

static bool checkreturn pb_decode_proto(pb_istream_t *stream, json_t *proto,
    json_t *protos, const char *key, json_t *result) {
  json_t *type, *_messages, *sub_msg, *sub_value;
  const char *type_text;
  type = json_object_get(proto, "type");
  type_text = json_string_value(type);
  uint64_t int_value;
  int64_t sint_value;
  float float_value;
  double double_value;
  uint32_t str_len;
  char *str_value;
#ifdef PB_DEBUG
  //printf("%s\n",json_dumps(result,JSON_ENCODE_ANY));
#endif
  _messages = json_object_get(protos, "__messages");
  switch (pb__get_type(type_text)) {
  case PB_uInt32:
    if (!pb_decode_varint(stream, &int_value)) {
      return false;
    }
    json_object_set(result, key, json_integer(int_value));
    break;
  case PB_int32:
  case PB_sInt32:
    if (!pb_decode_svarint(stream, &sint_value)) {
      return false;
    }
    json_object_set(result, key, json_integer(sint_value));
    break;
  case PB_float:
    if (!pb_decode_fixed32(stream, &float_value)) {
      return false;
    }
    json_object_set(result, key, json_real(float_value));
    break;
  case PB_double:
    if (!pb_decode_fixed64(stream, &double_value)) {
      return false;
    }
    json_object_set(result, key, json_real(double_value));
    break;
  case PB_string:
    if(!pb_decode_strlen(stream, &str_len)) {
      return false;
    }
    str_value = malloc(str_len + 1);
    if(str_value == NULL) {
      return false;
    }
    if (!pb_decode_string(stream, str_value, str_len)) {
      return false;
    }
    json_object_set(result, key, json_string(str_value));
    free(str_value);
    break;
  default:
    if (_messages) {
      sub_msg = json_object_get(_messages, type_text);
      if (sub_msg) {
        if (!key) {
          if (!pb_decode_submessage(stream, sub_msg, result)) {
            return false;
          }
        } else {
          sub_value = json_object();
          if (!pb_decode_submessage(stream, sub_msg, sub_value)) {
            return false;
          }
          json_object_set(result, key, sub_value);
        }
      } else {
        return false;
      }
    }

  }
  return true;
}

static bool checkreturn pb_decode_array(pb_istream_t *stream, json_t *proto, json_t *protos,
    const char *key, json_t *result) {
  json_t *type, *array, *value;
  const char *type_text;
  uint32_t size;
  int i;
  type = json_object_get(proto, "type");
  type_text = json_string_value(type);
  if (!result) {
    fprintf(stderr, "error result is null pb_decode_array\n");
    return false;
  }
  array = json_object_get(result, key);

#ifdef PB_DEBUG
  fprintf(stderr, "%s\n", json_dumps(proto,JSON_ENCODE_ANY));
#endif
  if (!array) {
    array = json_array();
  }

  if (pb__get_type(type_text)) {
    if (!pb_decode_varint32(stream, &size))
      return false;
    for (i = 0; i < size; i++) {
      value = json_object();
      if (!pb_decode_proto(stream, proto, protos, key, value))
        return false;
      json_array_append(array, value);
    }
  } else {
    value = json_object();
    if (!pb_decode_proto(stream, proto, protos, NULL, value))
      return false;
    json_array_append(array, value);
  }

  json_object_set(result, key, array);

  return true;
}

/*********************
 * Decode all fields *
 *********************/

static bool checkreturn pb_decode(pb_istream_t *stream, json_t *protos, json_t *result) {
  while (stream->bytes_left) {
    uint32_t tag;
    int wire_type;
    bool eof;
    json_t *tags, *_tag, *option, *proto;
    const char *name;
    const char *option_text;
    if (!pb_decode_tag(stream, &wire_type, &tag, &eof)) {
      if (eof)
        break;
      else
        return false;
    }
    char buffer[64];
    memset(&buffer, 0, 64);
    tags = json_object_get(protos, "__tags");
    if (!tags)
      return false;

    sprintf(buffer, "%u", tag);
    _tag = json_object_get(tags, buffer);
    if (!_tag)
      return false;
    name = json_string_value(_tag);
    proto = json_object_get(protos, name);
    if (!proto)
      return false;
    option = json_object_get(proto, "option");
    option_text = json_string_value(option);
    if (strcmp(option_text, "optional") == 0
        || strcmp(option_text, "required") == 0) {
      if (!pb_decode_proto(stream, proto, protos, name, result))
        return false;
    } else if (strcmp(option_text, "repeated") == 0) {
      if (!pb_decode_array(stream, proto, protos, name, result))
        return false;
    }
  }

  return true;
}

/* Field decoders */

/********************
 * Helper functions *
 ********************/

static bool checkreturn pb_decode_varint32(pb_istream_t *stream, uint32_t *dest) {
  uint64_t temp;
  bool status = pb_decode_varint(stream, &temp);
  if(status) {
    *dest = (uint32_t) temp;
  } else {
    *dest = -1;
  }
  return status;
}

/* Decode an integer in the varint format. This works for bool, enum, int32,
 * int64, uint32 and uint64 field types. */
bool checkreturn pb_decode_varint(pb_istream_t *stream, uint64_t *dest) {
  uint8_t byte;
  int bitpos = 0;
  *dest = 0;

  while (bitpos < 64 && pb_read(stream, &byte, 1)) {
    *dest |= (uint64_t)(byte & 0x7F) << bitpos;
    bitpos += 7;

    if (!(byte & 0x80))
      return true;
  }

  PB_RETURN_ERROR(stream, "varint overflow");
}

/* Decode an integer in the zig-zagged svarint format. This works for sint32
 * and sint64. */
static bool pb_decode_svarint(pb_istream_t *stream, int64_t *dest) {
  uint64_t value;
  if (!pb_decode_varint(stream, &value))
    return false;

  if (value & 1)
    *dest = (int64_t) (~(value >> 1));
  else
    *dest = (int64_t) (value >> 1);

  return true;
}

/* Decode a fixed32, sfixed32 or float value. You need to pass a pointer to
 * a 4-byte wide C variable. */
static bool pb_decode_fixed32(pb_istream_t *stream, void *dest) {
#ifdef __BIG_ENDIAN__
  uint8_t *bytes = (uint8_t*)dest;
  uint8_t lebytes[4];

  if (!pb_read(stream, lebytes, 4))
  return false;

  bytes[0] = lebytes[3];
  bytes[1] = lebytes[2];
  bytes[2] = lebytes[1];
  bytes[3] = lebytes[0];
  return true;
#else
  return pb_read(stream, (uint8_t*) dest, 4);
#endif
}

/* Decode a fixed64, sfixed64 or double value. You need to pass a pointer to
 * a 8-byte wide C variable. */
static bool pb_decode_fixed64(pb_istream_t *stream, void *dest) {
#ifdef __BIG_ENDIAN__
  uint8_t *bytes = (uint8_t*)dest;
  uint8_t lebytes[8];

  if (!pb_read(stream, lebytes, 8))
  return false;

  bytes[0] = lebytes[7];
  bytes[1] = lebytes[6];
  bytes[2] = lebytes[5];
  bytes[3] = lebytes[4];
  bytes[4] = lebytes[3];
  bytes[5] = lebytes[2];
  bytes[6] = lebytes[1];
  bytes[7] = lebytes[0];
  return true;
#else
  return pb_read(stream, (uint8_t*) dest, 8);
#endif
}

static bool checkreturn pb_decode_strlen(pb_istream_t *stream, uint32_t *size) {
  return pb_decode_varint32(stream, size);
}

/* Decode a string */
static bool checkreturn pb_decode_string(pb_istream_t *stream,
    void *dest, uint32_t size) {
  bool status;

  status = pb_read(stream, (uint8_t*) dest, size);
  *((uint8_t*) dest + size) = 0;
  return status;
}

/* Decode the tag for the next field in the stream. Gives the wire type and
 * field tag. At end of the message, returns false and sets eof to true. */
static bool checkreturn pb_decode_tag(pb_istream_t *stream, int *wire_type,
    uint32_t *tag, bool *eof) {
  uint32_t temp;
  *eof = false;
  *wire_type = 0;
  *tag = 0;

  if (!pb_decode_varint32(stream, &temp)) {
    if (stream->bytes_left == 0)
      *eof = true;

    return false;
  }

  if (temp == 0) {
    *eof = true; /* Special feature: allow 0-terminated messages. */
    return false;
  }

  *tag = temp >> 3;
  *wire_type = (temp & 7);
  return true;
}

/* Decode string length from stream and return a substream with limited length.
 * Remember to close the substream using pb_close_string_substream().
 */
static bool checkreturn pb_make_string_substream(pb_istream_t *stream,
    pb_istream_t *substream) {
  uint32_t size;
  if (!pb_decode_varint32(stream, &size)) {
    return false;
  }

  *substream = *stream;
  if (substream->bytes_left < size)
    PB_RETURN_ERROR(stream, "parent stream too short");

  substream->bytes_left = size;
  stream->bytes_left -= size;
  return true;
}

static void pb_close_string_substream(pb_istream_t *stream, pb_istream_t *substream) {
  stream->state = substream->state;
}

/* Decode submessage in __messages protos */
static bool checkreturn pb_decode_submessage(pb_istream_t *stream, json_t *protos,
    void *dest) {
  bool status;
  pb_istream_t substream;

  if (!pb_make_string_substream(stream, &substream)) {
    return false;
  }
  /* New array entries need to be initialized, while required and optional
   * submessages have already been initialized in the top-level pb_decode. */
  status = pb_decode(&substream, protos, dest);

  pb_close_string_substream(stream, &substream);
  return status;
}
