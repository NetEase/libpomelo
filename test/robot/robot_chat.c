#include <pomelo.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GATE_HOST "127.0.0.1"
#define GATE_PORT 3014
#define MAX_LINE_CHARS 1024
#define MAX_RUN_NUM 5000000
#define END_STR "bye"
#define ROBOT_STR "robot"

static const char *connectorHost = "";
static int connectorPort = 0;
static const char *user = "";
static const char *channel = "";
static pc_client_t *pomelo_client;

static void on_gate_close(pc_client_t *client, const char *event, void *data);
static void on_connector_close(pc_client_t *client, const char *event, void *data);
static void on_disconnect(pc_client_t *client, const char *event, void *data);
static void on_chat(pc_client_t *client, const char *event, void *data);
static void on_add(pc_client_t *client, const char *event, void *data);
static void on_leave(pc_client_t *client, const char *event, void *data);
static void on_request_connector_cb(pc_request_t *req, int status, json_t *resp);
static void on_send_cb(pc_request_t *req, int status, json_t *resp);
static void login(const char *username, const char *Channel);
static void msg_send(const char *message, const char *rid, const char *from, const char *target);

void on_request_gate_cb(pc_request_t *req, int status, json_t *resp) {
    if (status == -1) {
        printf("Fail to send request to server.\n");
    } else if (status == 0) {
        connectorHost = json_string_value(json_object_get(resp, "host"));
        connectorPort = json_number_value(json_object_get(resp, "port"));
        pc_client_t *client = pc_client_new();

        struct sockaddr_in address;

        memset(&address, 0, sizeof(struct sockaddr_in));
        address.sin_family = AF_INET;
        address.sin_port = htons(connectorPort);
        address.sin_addr.s_addr = inet_addr(connectorHost);

        // add pomelo events listener
        pc_add_listener(client, "disconnect", on_disconnect);
        pc_add_listener(client, "onChat", on_chat);
        pc_add_listener(client, "onAdd", on_add);
        pc_add_listener(client, "onLeave", on_leave);

        printf("try to connect to connector server %s %d\n", connectorHost, connectorPort);
        // try to connect to server.
        if (pc_client_connect(client, &address)) {
            printf("fail to connect connector server.\n");
            pc_client_destroy(client);
            return ;
        }

        const char *route = "connector.entryHandler.enter";
        json_t *msg = json_object();
        json_t *str = json_string(user);
        json_t *channel_str = json_string(channel);
        json_object_set_new(msg, "username", str);
        json_object_set_new(msg, "rid", channel_str);

        pc_request_t *request = pc_request_new();
        printf("%s %s\n", user, channel);
        pc_request(client, request, route, msg, on_request_connector_cb);
    }

    // release relative resource with pc_request_t
    json_t *pc_msg = req->msg;
    pc_client_t *pc_client = req->client;
    json_decref(pc_msg);
    pc_request_destroy(req);

    pc_client_stop(pc_client);
}

void on_request_connector_cb(pc_request_t *req, int status, json_t *resp) {
    printf("on_request_connector_cb\n");
    if (status == -1) {
        printf("Fail to send request to server.\n");
    } else if (status == 0) {
        char *json_str = json_dumps(resp, 0);
        printf("server response: %s \n", json_str);
        json_t *users = json_object_get(resp, "users");
        if (json_object_get(resp, "error") != NULL) {
            printf("connect error %s", json_str);
            free(json_str);
            return;
        }
        pomelo_client = req->client;
        printf("login chat ok\n");
    }

    // release relative resource with pc_request_t
    json_t *msg = req->msg;
    pc_client_t *client = req->client;
    json_decref(msg);
    pc_request_destroy(req);
}

void on_send_cb(pc_request_t *req, int status, json_t *resp) {
    if(status == 0){
        printf("on_send_cb ok\n");
    } else {
        printf("on_send_cb bad\n");
    }
}

void on_gate_close(pc_client_t *client, const char *event, void *data) {
    printf("gate client closed: %d.\n", client->state);
}

void on_connector_close(pc_client_t *client, const char *event, void *data) {
    printf("gate client closed: %d.\n", client->state);
}

void on_disconnect(pc_client_t *client, const char *event, void *data) {
    printf("%s\n", event);
}

void on_chat(pc_client_t *client, const char *event, void *data) {
    json_t *json = (json_t * )data;
    const char *msg = json_dumps(json, 0);
    printf("%s %s\n", event, msg);
}

void on_add(pc_client_t *client, const char *event, void *data) {
    json_t *json = (json_t * )data;
    json_t *user = json_object_get(json, "user");
    const char *msg = json_string_value(user);
    printf("%s %s\n", event, msg);
}

void on_leave(pc_client_t *client, const char *event, void *data) {
    json_t *json = (json_t * )data;
    json_t *user = json_object_get(json, "user");
    const char *msg = json_string_value(user);
    printf("%s %s\n", event, msg);
}

void login(const char *username, const char *Channel) {
    const char *ip = GATE_HOST;
    int port = GATE_PORT;
    user = username;
    channel = Channel;

    pc_client_t *client = pc_client_new();

    // add some event callback.
    pc_add_listener(client, PC_EVENT_DISCONNECT, on_gate_close);

    struct sockaddr_in address;

    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip);

    printf("try to connect to gate server %s %d\n", ip, port);

    // try to connect to server.
    if (pc_client_connect(client, &address)) {
        printf("fail to connect gate server.\n");
        pc_client_destroy(client);
        return;
    }

    const char *route = "gate.gateHandler.queryEntry";
    json_t *msg = json_object();
    json_t *str = json_string(username);
    json_object_set_new(msg, "uid", str);

    pc_request_t *request = pc_request_new();
    pc_request(client, request, route, msg, on_request_gate_cb);
}

void msg_send(const char *message, const char *rid, const char *from, const char *target) {
    const char *route = "chat.chatHandler.send";
    json_t *msg = json_object();
    json_t *str = json_string(message);
    json_object_set_new(msg, "content", str);
    json_object_set_new(msg, "rid", json_string(rid));
    json_object_set_new(msg, "from", json_string(from));
    json_object_set_new(msg, "target", json_string(target));

    pc_request_t *request = pc_request_new();
    pc_request(pomelo_client, request, route, msg, on_send_cb);
}

void run_robot() {
    int i = 0;
    for (i = 0; i < MAX_RUN_NUM; i++) {
        printf("send %d\n", i+1);
        char str[10] = "test";
        char tmp[10];
        sprintf(tmp, "%d", i);
        strcat(str, tmp); 
        msg_send(str, channel, user, "*");
    #ifdef _WIN32
        Sleep(1000);
    #else
        sleep(1);
    }
    printf("run_robot done\n");
}

int main() {
    printf("input username and channel\n");
    char input[MAX_LINE_CHARS];
    char s1[MAX_LINE_CHARS];
    char s2[MAX_LINE_CHARS];
    scanf("%s", s1);
    user = s1;
    scanf("%s", s2);
    channel = s2;

    login(user, channel);
    printf("\nInput a line to send message to server \ninput `robot` to run robot \ninput `bye` to exit.\n\n");
    while (scanf("%s", input) != EOF) {
        if (!strcmp(input, END_STR)) {
            break;
        }
        if (!strcmp(input, ROBOT_STR)) {
            run_robot();
            continue;
        }
        msg_send(input, channel, user, "*");
    }
    return 0;
}