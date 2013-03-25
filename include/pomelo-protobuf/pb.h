#ifndef _PB_H_
#define _PB_H_

/* pb.h: protobuf api for pomelo c client library.
 */

#define PB_VERSION 1.1.1

#include <stdint.h>
#include <stddef.h>
#include "jansson.h"

/*
 * protobuf encode
 */
int pc_pb_encode(uint8_t *buf, size_t len, size_t *written,
                  json_t *protos, json_t *msg);

/*
 * protobuf decode
 */
int pc_pb_decode(uint8_t *buf, size_t len, json_t *protos,
                  json_t *result);

#endif