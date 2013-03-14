#ifndef PC_TRANSPORT_H
#define PC_TRANSPORT_H

#include <uv.h>
#include <pomelo-client.h>

pc_transport_t *pc_transport_new(pc_client_t *client);

void pc_transport_destroy(pc_transport_t *transport);

int pc_transport_start(pc_transport_t *transport);

void pc_tp_on_tcp_read(uv_stream_t *socket, ssize_t nread, uv_buf_t buf);

#endif /* PC_TRANSPORT_H */