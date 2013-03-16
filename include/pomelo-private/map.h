#ifndef PC_MAP_H
#define PC_MAP_H

#include <stdlib.h>
#include "pomelo-private/ngx-queue.h"

/**
 * Simple hash map implements.
 */

#define PC__MAP_DEFAULT_CAPACITY 64

typedef struct pc_map_s pc_map_t;
typedef struct pc__pair_s pc__pair_t;

typedef void (*pc_map_value_release)(pc_map_t *map, const char *key,
                                     void *value);

struct pc_map_s {
  size_t capacity;
  ngx_queue_t *buckets;
  pc_map_value_release release_value;
};

struct pc__pair_s {
  const char *key;
  void *value;
  ngx_queue_t queue;
};

pc_map_t *pc_map_new(size_t capacity, pc_map_value_release release_value);

int pc_map_init(pc_map_t *map, size_t capacity, pc_map_value_release release_value);

void pc_map_destroy(pc_map_t *map);

void pc_map_close(pc_map_t *map);

int pc_map_set(pc_map_t *map, const char *key, void *value);

void *pc_map_get(pc_map_t *map, const char *key);

void *pc_map_del(pc_map_t *map, const char *key);

void pc_map_clear(pc_map_t *map);

#endif
