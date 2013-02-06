#ifndef _PB_H_
#define _PB_H_

/* pb.h: protobuf api for pomelo c client library.
 */

#define PB_VERSION 0.0.1

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <jansson.h>

#include "pb_encode.h"
#include "pb_decode.h"
#include "pb_util.h"

/* Encode struct to given output stream.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by src_struct must match the description in fields.
 * All required fields in the struct are assumed to have been filled in.
 */
 bool pc_pb_encode(uint8_t *buf, size_t len, size_t *written, json_t *protos,
		json_t *msg);

/* Decode from stream to destination struct.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by dest must match the description in fields.
 */
 bool pc_pb_decode(uint8_t *buf, size_t len, json_t *protos, json_t *result);

#endif
