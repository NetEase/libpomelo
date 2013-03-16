#ifndef PC_INTERNAL_H
#define PC_INTERNAL_H

#include "pomelo.h"

int pc__handshake_resp(pc_client_t *client, const char *data, size_t len);

int pc__heartbeat(pc_client_t *client);

void pc__heartbeat_cb(uv_timer_t* heartbeat_timer, int status);

void pc__timeout_cb(uv_timer_t* timeout_timer, int status);

#endif /* PC_INTERNAL_H */