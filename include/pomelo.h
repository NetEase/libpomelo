#ifndef POMELO_CLIENT_H
#define POMELO_CLIENT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  /* Windows - set up dll import/export decorators. */
# if defined(BUILDING_PC_SHARED)
    /* Building shared library. */
#   define PC_EXTERN __declspec(dllexport)
# elif defined(USING_PC_SHARED)
    /* Using shared library. */
#   define PC_EXTERN __declspec(dllimport)
# else
    /* Building static library. */
#   define PC_EXTERN /* nothing */
# endif
#elif __GNUC__ >= 4
# define PC_EXTERN __attribute__((visibility("default")))
#else
# define PC_EXTERN /* nothing */
#endif

#include "uv.h"
#include "jansson.h"
#include "pomelo-private/map.h"
#include "pomelo-protocol/package.h"

#define PC_TYPE "c"
#define PC_VERSION "0.0.1"

#define PC_EVENT_DISCONNECT "disconnect"

#define PC_DEFAULT_HEARTBEAT 3000
#define PC_DEFAULT_TIMEOUT (PC_DEFAULT_HEARTBEAT + 5000)

#define PC_HANDSHAKE_OK 200

#define PC_HEARTBEAT_TIMEOUT_FACTOR 2

typedef struct pc_client_s pc_client_t;
typedef struct pc_req_s pc_req_t;
typedef struct pc_connect_s pc_connect_t;
typedef struct pc_tcp_req_s pc_tcp_req_t;
typedef struct pc_request_s pc_request_t;
typedef struct pc_notify_s pc_notify_t;
typedef struct pc_msg_s pc_msg_t;

typedef void (*pc_event_cb)(pc_client_t *client, const char *event, void *data);
typedef void (*pc_connect_cb)(pc_connect_t* req, int status);
typedef void (*pc_request_cb)(pc_request_t *, int, json_t *);
typedef void (*pc_notify_cb)(pc_notify_t *, int);
typedef int (*pc_handshake_cb)(pc_client_t *, json_t *);
typedef void (*pc_handshake_req_cb)(pc_client_t *, pc_connect_t *, int);
typedef pc_msg_t *(*pc_msg_parse_cb)(pc_client_t *, const char *, int);
typedef void (*pc_msg_parse_done_cb)(pc_client_t *, pc_msg_t *);
typedef pc_buf_t (*pc_msg_encode_cb)(pc_client_t *, uint32_t reqId,
                                     const char* route, json_t *msg);
typedef void (*pc_msg_encode_done_cb)(pc_client_t *, pc_buf_t buf);

/**
 * Pomelo client states.
 */
typedef enum {
  PC_ST_INITED = 1,
  PC_ST_CONNECTING,
  PC_ST_CONNECTED,
  PC_ST_WORKING,
  PC_ST_CLOSED
} pc_client_state;

/**
 * Pomelo client async request types.
 */
typedef enum {
  PC_CONNECT,
  PC_REQUEST,
  PC_NOTIFY
} pc_req_type;

typedef enum {
  PC_TP_ST_INITED = 1,
  PC_TP_ST_WORKING,
  PC_TP_ST_CLOSED
} pc_transport_state;

typedef struct {
  pc_client_t *client;
  uv_tcp_t *socket;
  pc_transport_state state;
} pc_transport_t;

#define PC_REQ_FIELDS                                                         \
  /* private */                                                               \
  pc_client_t *client;                                                        \
  pc_transport_t *transport;                                                  \
  pc_req_type type;                                                           \
  void *data;                                                                 \

#define PC_TCP_REQ_FIELDS                                                     \
  /* public */                                                                \
  const char *route;                                                          \
  json_t *msg;                                                                \

/**
 * The abstract base class of all async request in Pomelo client.
 */
struct pc_req_s {
  PC_REQ_FIELDS
};

/**
 * The abstract base class of all tcp async request and a subclass of pc_req_t.
 */
struct pc_tcp_req_s {
  PC_REQ_FIELDS
  PC_TCP_REQ_FIELDS
};

/**
 * Pomelo client instance
 */
struct pc_client_s {
  /* public */
  pc_client_state state;
  /* private */
  uv_loop_t *uv_loop;
  pc_transport_t *transport;
  pc_map_t *listeners;
  pc_map_t *requests;
  pc_pkg_parser_t *pkg_parser;
  int heartbeat;
  int timeout;
  json_t *handshake_opts;
  pc_handshake_cb handshake_cb;
  pc_connect_t *conn_req;
  json_t *route_to_code;
  json_t *code_to_route;
  json_t *server_protos;
  json_t *client_protos;
  pc_msg_parse_cb parse_msg;
  pc_msg_parse_done_cb parse_msg_done;
  pc_msg_encode_cb encode_msg;
  pc_msg_encode_done_cb encode_msg_done;
  uv_timer_t heartbeat_timer;
  uv_timer_t timeout_timer;
  uv_async_t close_async;
  uv_mutex_t mutex;
  uv_cond_t cond;
  uv_mutex_t listener_mutex;
  uv_thread_t worker;
};

/**
 * Connect request class is a subclass of pc_req_t.
 * Connect is the async context for a connection request to server.
 */
struct pc_connect_s {
  PC_REQ_FIELDS
  /* public */
  struct sockaddr_in *address;
  pc_connect_cb cb;
  /* private */
  uv_tcp_t *socket;
};

/**
 * Pomelo request class is a subclass of pc_tcp_req_t.
 * Request is the async context for a Pomelo request to server.
 */
struct pc_request_s {
  PC_REQ_FIELDS
  PC_TCP_REQ_FIELDS
  uint32_t id;
  pc_request_cb cb;
  ngx_queue_t queue;
};

/**
 * Pomelo notify class is a subclass of pc_tcp_req_t.
 * Notify is the async context for a Pomelo notify to server.
 */
struct pc_notify_s {
  PC_REQ_FIELDS
  PC_TCP_REQ_FIELDS
  pc_notify_cb cb;
};

struct pc_msg_s {
  uint32_t id;
  const char* route;
  json_t *msg;
};

/**
 * Create and initiate Pomelo client intance.
 *
 * @return Pomelo client instance
 */
pc_client_t *pc_client_new();

/**
 * Create and initiate connect request instance.
 *
 * @param  address server address
 * @return         connect request instance
 */
pc_connect_t *pc_connect_req_new(struct sockaddr_in *address);

/**
 * Destroy a connect request instance and release inner resources.
 *
 * @param  req connect request instance.
 */
void pc_connect_req_destroy(pc_connect_t *req);

/**
 * Connect Pomelo client to server.
 *
 * @param  client         Pomelo client instance
 * @param  req            connection request
 * @param  handshake_opts handshake options send to server
 * @param  cb             connection established callback
 * @return                result code, 0 for ok, -1 for error
 */
int pc_connect(pc_client_t *client, pc_connect_t *req,
               json_t *handshake_opts, pc_connect_cb cb);

/**
 * Create and initiate a request instance.
 *
 * @return     req request instance
 */
pc_request_t *pc_request_new();

/**
 * Destroy and release inner resource of a request instance.
 *
 * @param req request instance to be destroied.
 */
void pc_request_destroy(pc_request_t *req);

/**
 * Send rerquest to server.
 * The message object and request object must keep
 * until the pc_request_cb invoked.
 *
 * @param  client Pomelo client instance
 * @param  req    initiated request instance
 * @param  route  route string
 * @param  msg    message object
 * @param  cb     request callback
 * @return        0 or -1
 */
int pc_request(pc_client_t *client, pc_request_t *req, const char *route,
               json_t *msg, pc_request_cb cb);

/**
 * Create and initiate notify instance.
 *
 * @return   notify instance
 */
pc_notify_t *pc_notify_new();

/**
 * Destroy and release inner resource of a notify instance.
 *
 * @param req notify instance to be destroied.
 */
void pc_notify_destroy(pc_notify_t *req);

/**
 * Send notify to server.
 * The message object and notify object must keep
 * until the pc_notify_cb invoked.
 *
 * @param  client Pomelo client instance
 * @param  req    initiated notify instance
 * @param  route  route string
 * @param  msg    message object
 * @param  cb     notify callback
 * @return        0 or -1
 */
int pc_notify(pc_client_t *client, pc_notify_t *req, const char *route,
              json_t *msg, pc_notify_cb cb);

/**
 * Disconnect Pomelo client and reset all status back to initialted.
 *
 * @param client Pomelo client instance.
 */
void pc_disconnect(pc_client_t *client, int reset);

int pc_run(pc_client_t *client);

int pc_add_listener(pc_client_t *client, const char *event,
                    pc_event_cb event_cb);

int pc_client_connect(pc_client_t *client, struct sockaddr_in *addr);

void pc_client_destroy(pc_client_t *client);

int pc_client_join(pc_client_t *client);

void pc_client_stop(pc_client_t *client);

void pc_emit_event(pc_client_t *client, const char *event, void *data);

/* Don't export the private CPP symbols. */
#undef PC_TCP_REQ_FIELDS
#undef PC_REQ_FIELDS

#ifdef __cplusplus
}
#endif
#endif /* POMELO_CLIENT_H */
