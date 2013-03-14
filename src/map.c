#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <hashtable.h>
#include <pomelo-private/map.h>

pc_map_t *pc_map_new(pc_map_value_release release_value) {
  pc_map_t *map = (pc_map_t *)malloc(sizeof(pc_map_t));
  if(map == NULL) {
    return NULL;
  }

  memset(map, 0, sizeof(pc_map_t));

  if(pc_map_init(map, release_value)) {
    free(map);
    return NULL;
  }

  return map;
}

void pc_map_destroy(pc_map_t *map) {
  pc_map_close(map);
  free(map);
}

int pc_map_init(pc_map_t *map, pc_map_value_release release_value) {
  map->table = (hashtable_t *)malloc(sizeof(hashtable_t));
  if(map->table == NULL) {
    fprintf(stderr, "Fail to malloc hashtable.\n");
    return -1;
  }

  if(hashtable_init(map->table)) {
    free(map->table);
    map->table = NULL;
    return -1;
  }

  map->release_value = release_value;

  return 0;
}

void pc_map_close(pc_map_t *map) {
  if(map->table == NULL) return;

  if(map->release_value) {
    for(void *iter = hashtable_iter(map->table); iter;
        iter = hashtable_iter_next(map->table, iter)) {
      map->release_value(map, hashtable_iter_key(iter),
                         (void *)json_integer_value(hashtable_iter_value(iter)));
    }
  }
  hashtable_close(map->table);
  free(map->table);
  map->table = NULL;
}

int pc_map_set(pc_map_t *map, const char *key, void *value) {
  return hashtable_set(map->table, key, 0, json_integer((json_int_t)value));
}

void *pc_map_get(pc_map_t *map, const char *key) {
  json_t *value = hashtable_get(map->table, key);
  if(value == NULL) {
    return NULL;
  }
  return (void *)json_integer_value(value);
}

int pc_map_del(pc_map_t *map, const char *key) {
  return hashtable_del(map->table, key);
}

void pc_map_clear(pc_map_t *map) {
  if(map->release_value) {
    for(void *iter = hashtable_iter(map->table); iter;
        iter = hashtable_iter_next(map->table, iter)) {
      map->release_value(map, hashtable_iter_key(iter),
                         (void *)json_integer_value(hashtable_iter_value(iter)));
    }
  }
  hashtable_clear(map->table);
}
