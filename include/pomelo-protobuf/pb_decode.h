#ifndef _PB_DECODE_H_
#define _PB_DECODE_H_

/* pb_decode.h: Functions to decode protocol buffers. Depends on pb_decode.c.
 * The main function is pb_decode. You will also need to create an input
 * stream, which is easiest to do with pb_istream_from_buffer().
 *
 */

#include <stdbool.h>
#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

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
struct _pb_istream_t {
	bool (*callback)(pb_istream_t *stream, uint8_t *buf, size_t count);
	void *state; /* Free field for use by callback implementation */
	size_t bytes_left;

#ifndef PB_NO_ERRMSG
	const char *errmsg;
#endif
};

bool pb_decode(pb_istream_t *stream, json_t *protos, json_t *result);

pb_istream_t pb_istream_from_buffer(uint8_t *buf, size_t bufsize);
bool pb_read(pb_istream_t *stream, uint8_t *buf, size_t count);

/* --- Helper functions ---
 * You may want to use these from your caller or callbacks.
 */

bool pb_decode_proto(pb_istream_t *stream, json_t *proto, json_t *protos,
		const char *key, json_t *result);

bool pb_decode_array(pb_istream_t *stream, json_t *proto, json_t *protos,
		const char *key, json_t *result);

/* Decode the tag for the next field in the stream. Gives the wire type and
 * field tag. At end of the message, returns false and sets eof to true. */
bool pb_decode_tag(pb_istream_t *stream, int *wire_type, uint32_t *tag, bool *eof);

/* Decode an integer in the varint format. This works for bool, enum, int32,
 * int64, uint32 and uint64 field types. */
bool pb_decode_varint(pb_istream_t *stream, uint64_t *dest);

bool pb_decode_varint32(pb_istream_t *stream, uint32_t *dest);

/* Decode an integer in the zig-zagged svarint format. This works for sint32
 * and sint64. */
bool pb_decode_svarint(pb_istream_t *stream, int64_t *dest);

/* Decode a fixed32, sfixed32 or float value. You need to pass a pointer to
 * a 4-byte wide C variable. */
bool pb_decode_fixed32(pb_istream_t *stream, void *dest);

/* Decode a fixed64, sfixed64 or double value. You need to pass a pointer to
 * a 8-byte wide C variable. */
bool pb_decode_fixed64(pb_istream_t *stream, void *dest);

/* Make a limited-length substream for reading a PB_string field. */
bool pb_make_string_substream(pb_istream_t *stream, pb_istream_t *substream);
void pb_close_string_substream(pb_istream_t *stream, pb_istream_t *substream);

/* Decode a string */bool pb_decode_string(pb_istream_t *stream, void *dest);

/* Decode submessage in __messages protos */
bool pb_decode_submessage(pb_istream_t *stream, json_t *protos, void *dest);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
