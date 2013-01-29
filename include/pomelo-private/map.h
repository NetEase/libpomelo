#ifndef PC_MAP_H
#define PC_MAP_H

#include <jansson.h>
#include <hashtable.h>

/**
 * Wrapper for hashtable from jansson.
 */

typedef struct pc_map_s pc_map_t;

struct pc_map_s {
  hashtable_t table;
};

pc_map_t *pc_map_new();

int pc_map_init(pc_map_t *map);

void pc_map_destroy(pc_map_t *map);

void pc_map_close(pc_map_t *map);

int pc_map_set(pc_map_t *map, const char *key, void *value);

void *pc_map_get(pc_map_t *map, const char *key);

int pc_map_del(pc_map_t *map, const char *key);

void pc_map_clear(pc_map_t *map);

void *pc_map_iter(pc_map_t *map);

void *pc_map_iter_at(pc_map_t *map, const char *key);

void *pc_map_iter_next(pc_map_t *map, void *iter);

void *pc_map_iter_key(void *iter);

void *pc_map_iter_value(void *iter);

#endif
