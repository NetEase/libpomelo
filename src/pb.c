#include "pb.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

bool pc_pb_encode(uint8_t *buf, size_t len, size_t *written, json_t *protos,
        json_t *msg) {
    pb_ostream_t stream = pb_ostream_from_buffer(buf, len);
    if (!pb_encode(&stream, protos, msg)) {
        printf("pb_encode error\n");
        return 0;
    }
    *written = stream.bytes_written;
    return 1;
}

bool pc_pb_decode(uint8_t *buf, size_t len, json_t *protos, json_t *result) {
    pb_istream_t stream = pb_istream_from_buffer(buf, len);
    if (!pb_decode(&stream, protos, result)) {
        printf("decode error\n");
        return 0;
    }
    return 1;
}

