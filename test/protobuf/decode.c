#include <pomelo-protobuf/pb.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    json_t *result, *protos, *msg;
    json_error_t error;

    protos = json_load_file("test.proto", 0, &error);
    //printf("%s\n", buffer);
    if (!protos) {
        printf("error load protos json\n");
        return 0;
    }

    msg = json_load_file("test.json", 0, &error);

    if (!msg) {
        printf("error load msg json\n");
        return 0;
    }

    uint8_t buffer1[512];
    //memset(buffer,0,sizeof(buffer));
    //pb_ostream_t stream1 = pb_ostream_from_buffer(buffer1, sizeof(buffer1));
    size_t written = 0;
    if (!pc_pb_encode(buffer1, sizeof(buffer1), &written, protos, msg)) {
        printf("pb_encode error\n");
        return 0;
    }

    //pb_istream_t stream = pb_istream_from_buffer(buffer1, stream1.bytes_written);

    result = json_object();
    if (!pc_pb_decode(buffer1, written, protos, result)) {
        printf("decode error\n");
        return 0;
    }

    printf("%f\n", json_number_value(json_object_get(result, "a")));
    printf("%s\n\n", json_dumps(result, JSON_INDENT(2)));

    result = json_loads(json_dumps(result, JSON_INDENT(2)), 0, &error);
    printf("%f\n", json_number_value(json_object_get(result, "a")));

    return 0;
}
