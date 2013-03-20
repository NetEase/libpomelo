##libpomelo

libpomelo is a C language client SDK for Pomelo, supporting the Pomelo
protocol definition in Pomelo 0.3 version.

 * Homepage: <http://pomelo.netease.com/>
 * Mailing list: <https://groups.google.com/group/pomelo>
 * Documentation: <http://github.com/NetEase/pomelo>
 * Wiki: <https://github.com/NetEase/pomelo/wiki/>
 * Issues: <https://github.com/NetEase/pomelo/issues/>
 * Tags: game, nodejs

##Dependencies

* [libuv](https://github.com/joyent/libuv) - Cross platform layer, mainly using Network IO and thread.
* [jansson](https://github.com/akheron/jansson) - JSON encode and decode library.

##Usage

###Create a client instance

``` c
// create a client instance.
pc_client_t *client = pc_client_new();
  
```

###Add listeners

``` c
  // add some event callback.
  pc_add_listener(client, "onHey", on_hey);
  pc_add_listener(client, PC_EVENT_DISCONNECT, on_close);
```

###Listener definition

``` c
// disconnect event callback.
void on_close(pc_client_t *client, const char *event, void *data) {
  printf("client closed: %d.\n", client->state);
}
```

###Connect to server

``` c
  struct sockaddr_in address;

  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = inet_addr(ip);

  // try to connect to server.
  if(pc_client_connect(client, &address)) {
    printf("fail to connect server.\n");
    pc_client_destroy(client);
    return 1;
  }
 ```

###Send a notify
 
 ``` c
// notified callback
void on_notified(pc_notify_t *req, int status) {
  if(status == -1) {
    printf("Fail to send notify to server.\n");
  } else {
    printf("Notify finished.\n");
  }

  // release resources
  json_t *msg = req->msg;
  json_decref(msg);
  pc_notify_destroy(req);
}

// send a notify
void do_notify(pc_client_t *client) {
  // compose notify.
  const char *route = "connector.helloHandler.hello";
  json_t *msg = json_object();
  json_t *json_str = json_string("hello");
  json_object_set(msg, "msg", json_str);
  // decref json string
  json_decref(json_str);

  pc_notify_t *notify = pc_notify_new();
  pc_notify(client, notify, route, msg, on_notified);
}
 ```

###Send a request
 
 ``` c
// request callback
void on_request_cb(pc_request_t *req, int status, json_t *resp) {
  if(status == -1) {
    printf("Fail to send request to server.\n");
  } else if(status == 0) {
    char *json_str = json_dumps(resp, 0);
    if(json_str != NULL) {
      printf("server response: %s\n", json_str);
      free(json_str);
    }
  }

  // release relative resource with pc_request_t
  json_t *msg = req->msg;
  pc_client_t *client = req->client;
  json_decref(msg);
  pc_request_destroy(req);

  pc_client_stop(client);
}

// send a request
void do_request(pc_client_t *client) {
  // compose request
  const char *route = "connector.helloHandler.hi";
  json_t *msg = json_object();
  json_t *str = json_string("hi~");
  json_object_set(msg, "msg", str);
  // decref for json object
  json_decref(str);

  pc_request_t *request = pc_request_new();
  pc_request(client, request, route, msg, on_request_cb);
}
 ```
 
###More example

The complete examples: [example/]()

Tcp server for test: [tcp-pomelo](https://github.com/changchang/tcp-pomelo). 
 
##API
 
More information about API, please refer to [pomelo.h](https://github.com/NetEase/libpomelo/blob/master/include/pomelo.h).


##Build

###Prerequisite

Install [GYP](http://code.google.com/p/gyp/source/checkout).

###Mac

```
./pomelo_gyp
xcodebuld -project pomelo.xcodeproj
```

###Linux
```
./pomelo_gyp
make
```

###Windows
Not test yet.