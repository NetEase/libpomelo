#include <pomelo-protobuf/pb.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    json_t *msg, *protos, *root, *value, *option, *proto, *result;
    json_error_t error;

    msg = json_load_file("rootMsg.json", 0, &error);
    // msg = json_load_file("msg.json", 0, &error);

    if (!msg) {
        printf("error load msg json\n");
        return 0;
    }

    protos = json_load_file("rootProtos.json", 0, &error);
    // protos = json_load_file("protos.json", 0, &error);
    uint8_t buffer[2000];
    uint8_t buffer1[2000];

    size_t written = 0;

    if (!protos) {
        printf("error load protos json\n");
        return 0;
    }

    root = msg;

    const char *option_text;
    const char *key;
    void *iter = json_object_iter(root);
    while (iter) {
        key = json_object_iter_key(iter);
        // printf("%s\n", key);
        value = json_object_iter_value(iter);

        proto = json_object_get(protos, key);
        // printf("%s\n", json_dumps(value, JSON_ENCODE_ANY));
        // printf("%s\n", json_dumps(proto, JSON_ENCODE_ANY));
        if (proto) {
            written = 0;
            memset(buffer, 0, sizeof(buffer));
            if (!pc_pb_encode(buffer, sizeof(buffer), &written, protos, proto, value)) {
                printf("pb_encode error\n");
                return 0;
            }
            result = json_object();
            if (!pc_pb_decode(buffer, written, protos, proto, result)) {
                printf("decode error\n");
                return 0;
            }
            printf("%s\n\n", json_dumps(result, JSON_INDENT(2)));
        } else {
        }
        iter = json_object_iter_next(root, iter);
    }

    return 0;
}