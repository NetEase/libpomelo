#ifndef PC_COMMON_H
#define PC_COMMON_H

#include "uv.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

void pc__handle_close_cb(uv_handle_t* handle);

#endif /* PC_COMMON_H */