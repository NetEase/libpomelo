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

#include <uv.h>
#include <pomelo-private/map.h>
#include <pomelo-protocol/package.h>

#define PC_VERSION "1.1.1"

#define PC_EVENT_DISCONNECT "disconnect"
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

#define PC_DEFAULT_HEARTBEAT 3000
#define PC_DEFAULT_TIMEOUT (PC_DEFAULT_HEARTBEAT + 5000)

#define PC_HANDSHAKE_OK 200

typedef struct pc_client_s pc_client_t;
typedef struct pc_req_s pc_req_t;
typedef struct pc_connect_s pc_connect_t;
typedef struct pc_tcp_req_s pc_tcp_req_t;
typedef struct pc_request_s pc_request_t;
typedef struct pc_notify_s pc_notify_t;
typedef struct pc_message_s pc_message_t;

typedef void (*pc_event_cb)(const char *, void *attach);
typedef void (*pc_connect_cb)(pc_connect_t* req, int status);
typedef void (*pc_request_cb)(pc_request_t *, int, pc_message_t *);
typedef void (*pc_notify_cb)(pc_notify_t *, int);
typedef int (*pc_handshake_cb)(pc_client_t *, json_t *);
typedef void (*pc_handshake_req_cb)(pc_client_t *, pc_connect_t *, int);

/**
 * Pomelo client states.
 */
typedef enum {
  PC_ST_INITED = 1,
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

#define PC_REQ_FIELDS                                                         \
  /* private */                                                               \
  pc_client_t *client;                                                        \
  pc_req_type type;                                                           \

#define PC_TCP_REQ_FIELDS                                                     \
  /* public */                                                                \
  const char *route;                                                          \
  pc_message_t *msg;                                                          \
  void *data;                                                                 \

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
  pc_client_state state;
  /* private */
  uv_loop_t *uv_loop;
  uv_tcp_t socket;
  pc_map_t listeners;
  pc_pkg_parser_t pkg_parser;
  int heartbeat;
  int timeout;
  uv_timer_t heartbeat_timer;
  uv_timer_t timeout_timer;
  json_t *handshake_opts;
  pc_handshake_cb handshake_cb;
  pc_connect_t *conn_req;
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
  pc_request_cb cb;
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

struct pc_message_s {
};

/**
 * Initiate Pomelo client intance.
 *
 * @param  client Pomelo client instance
 * @return        0 or -1
 */
int pc_client_init(pc_client_t *client);

/**
 * Initiate connect request instance.
 *
 * @param  req     connect request instance
 * @param  address server address
 * @return         0 or -1
 */
int pc_connect_init(pc_connect_t *req, struct sockaddr_in *address);

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
 * Initiate request instance.
 * The route string and message object must keep until the pc_notify_cb invoke.
 *
 * @param  req request instance
 * @param  route route string
 * @param  msg message object
 * @return     0 or -1
 */
int pc_request_init(pc_request_t *req, const char *route, pc_message_t *msg);

/**
 * Send rerquest to server.
 *
 * @param  client Pomelo client instance
 * @param  req    initiated request instance
 * @param  cb     request callback
 * @return        0 or -1
 */
int pc_request(pc_client_t *client, pc_request_t *req, pc_request_cb cb);

/**
 * Initiate notify instance.
 *
 * @param  req   notify instance
 * @return       0 or -1
 */
int pc_notify_init(pc_notify_t *req);

/**
 * Send notify to server.
 * The message object must keep until the pc_notify_cb invoke.
 *
 * @param  client Pomelo client instance
 * @param  req    initiated notify instance
 * @param  route  route string
 * @param  msg    message object
 * @param  cb     notify callback
 * @return        0 or -1
 */
int pc_notify(pc_client_t *client, pc_notify_t *req, const char *route,
              pc_message_t *msg, pc_notify_cb cb);

int pc_notify_cleanup(pc_notify_t *req);

int pc_disconnect(pc_client_t *client);

int pc_run(pc_client_t *client);

int pc_stop(pc_client_t *client);

/**
 * Return the default pomelo client loop instance.
 */
PC_EXTERN pc_client_t * pc_default_client(void);

/* Don't export the private CPP symbols. */
#undef PC_TCP_REQ_FIELDS
#undef PC_REQ_FIELDS

#ifdef __cplusplus
}
#endif
#endif /* POMELO_CLIENT_H */
