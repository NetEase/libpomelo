#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <hashtable.h>
#include <pomelo-private/map.h>

pc_map_t *pc_map_new() {
  pc_map_t *map = (pc_map_t *)malloc(sizeof(pc_map_t));
  if(map == NULL) {
    return NULL;
  }

  if(pc_map_init(map)) {
    free(map);
    return NULL;
  }

  return map;
}

void pc_map_destroy(pc_map_t *map) {
  // TODO: iterate and release listner lists
  hashtable_close(&map->table);
  free(map);
}

int pc_map_init(pc_map_t *map) {
  if(hashtable_init(&map->table)) {
    return -1;
  }

  return 0;
}

int pc_map_set(pc_map_t *map, const char *key, void *value) {
  return hashtable_set(&map->table, key, 0, value);
}

void *pc_map_get(pc_map_t *map, const char *key) {
  return hashtable_get(&map->table, key);
}

int pc_map_del(pc_map_t *map, const char *key) {
  return hashtable_del(&map->table, key);
}

void pc_map_clear(pc_map_t *map) {
  hashtable_clear(&map->table);
}

void *pc_map_iter(pc_map_t *map) {
  return hashtable_iter(&map->table);
}

void *pc_map_iter_at(pc_map_t *map, const char *key) {
  return hashtable_iter_at(&map->table, key);
}

void *pc_map_iter_next(pc_map_t *map, void *iter) {
  return hashtable_iter_next(&map->table, iter);
}

void *pc_map_iter_key(void *iter) {
  return hashtable_iter_key(iter);
}

void *pc_map_iter_value(void *iter) {
  return hashtable_iter_value(iter);
}