#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <jansson.h>
#include <pomelo-protobuf/pb.h>

#define WRITE_REQ_DATA  "Hello, world."
#define NUM_WRITE_REQS  1

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req;

uv_loop_t *loop;
write_req* write_reqs;
uv_tcp_t client;
uv_connect_t req;
uv_shutdown_t shutdown_req;

void on_connect(uv_connect_t* req, int status);
void on_write(uv_write_t* req, int status);
void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf);
void on_shutdown(uv_shutdown_t* req, int status);
void on_close(uv_handle_t* handle);
uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size);

void on_connect(uv_connect_t* req, int status) {
    printf("connect to tcp\n");
    int i;
    int r;
    write_req* w;
    uv_stream_t* stream;

    stream = req->handle;
    for (i = 0; i < NUM_WRITE_REQS; i++) {
        w = &write_reqs[i];
        r = uv_write(&w->req, stream, &w->buf, 1, on_write);
    }
    //printf("%d\n", status);
    //r = uv_shutdown(&shutdown_req, stream, on_shutdown);

    r = uv_read_start(stream, on_alloc, on_read);

    printf("%d\n", r);

}

uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size) {
    static char slab[65536];
    return uv_buf_init(slab, sizeof slab);
}

void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
    if (nread == -1) {
        if (uv_last_error(loop).code != UV_EOF)
            fprintf(stderr, "Read error %s\n",
                    uv_err_name(uv_last_error(loop)));
        uv_close((uv_handle_t*) tcp, NULL );
        return;
    }
    printf("read\n");
    printf("%d\n", buf.len);
    printf("%s\n", buf.base);
    char* text = buf.base;
    json_error_t error;
    json_t *data, *x, *y, *test;
    data = json_loads(text, 0, &error);
    if (!json_is_object(data)) {
        return;
    }
    x = json_object_get(data, "x");
    y = json_object_get(data, "y");
    test = json_object_get(data, "test");

    printf("x is %" JSON_INTEGER_FORMAT "\n", json_integer_value(x));
    printf("y is %" JSON_INTEGER_FORMAT "\n", json_integer_value(y));
    printf("%s\n", json_string_value(test));
    //uv_close((uv_handle_t*)tcp, on_close);
}

void on_write(uv_write_t* req, int status) {
    if (status == -1) {
        fprintf(stderr, "Write error %s\n", uv_err_name(uv_last_error(loop)));
    }
    printf("write\n");
    //free((write_req*)req);
}

void on_shutdown(uv_shutdown_t* req, int status) {
    //uv_close((uv_handle_t*)req->handle, on_close);
    free(write_reqs);
}

void on_close(uv_handle_t* handle) {
    printf("close it\n");
}

int main() {
    loop = uv_default_loop();
    json_t *msg, *protos;
    json_error_t error;

    msg = json_load_file("test.json", 0, &error);

    if (!msg) {
        printf("error load msg json\n");
        return 0;
    }

    protos = json_load_file("test.proto", 0, &error);

    if (!protos) {
        printf("error load protos json\n");
        return 0;
    }

    char buffer[512];
    //memset(buffer,0,sizeof(buffer));
    //pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    size_t written = 0;
    if (!pc_pb_encode(buffer, sizeof(buffer), &written, protos, msg)) {
        printf("pb_encode error\n");
        return 0;
    }

    //fwrite(buffer, 1, stream.bytes_written, stdout);
    write_reqs = malloc(sizeof(*write_reqs) * NUM_WRITE_REQS);
    int i;
    for (i = 0; i < NUM_WRITE_REQS; i++) {
        write_reqs[i].buf = uv_buf_init(buffer, written);
    }
    uv_tcp_init(loop, &client);
    //uv_tcp_keepalive(&client,1,1);
    //uv_tcp_nodelay(&client,1);
    struct sockaddr_in connect_addr = uv_ip4_addr("0.0.0.0", 7000);
    uv_tcp_connect(&req, &client, connect_addr, on_connect);
    uv_run(loop);

    return 0;
}
