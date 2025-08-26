#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VEC(type)                                                              \
  struct {                                                                     \
    type *data;                                                                \
    size_t size;                                                               \
    size_t cap;                                                                \
  }

#define vec_init(v)                                                            \
  do {                                                                         \
    (v).data = NULL;                                                           \
    (v).size = 0;                                                              \
    (v).cap = 0;                                                               \
  } while (0)

#define vec_free(v)                                                            \
  do {                                                                         \
    free((v).data);                                                            \
    (v).data = NULL;                                                           \
    (v).size = (v).cap = 0;                                                    \
  } while (0)

#define vec_reserve(v, n)                                                      \
  do {                                                                         \
    if ((v).cap < (n)) {                                                       \
      size_t new_cap = (n);                                                    \
      void *new_data = realloc((v).data, new_cap * sizeof(*(v).data));         \
      if (!new_data) {                                                         \
        perror("realloc");                                                     \
        exit(1);                                                               \
      }                                                                        \
      (v).data = new_data;                                                     \
      (v).cap = new_cap;                                                       \
    }                                                                          \
  } while (0)

#define vec_push_back(v, val)                                                  \
  do {                                                                         \
    if ((v).size == (v).cap) {                                                 \
      vec_reserve(v, (v).cap ? (v).cap * 2 : 4);                               \
    }                                                                          \
    (v).data[(v).size++] = (val);                                              \
  } while (0)

#define vec_pop_back(v)                                                        \
  do {                                                                         \
    if ((v).size > 0)                                                          \
      (v).size--;                                                              \
  } while (0)

#define vec_back(v) ((v).data[(v).size - 1])

#define vec_empty(v) ((v).size == 0)

#define vec_clear(v) ((v).size = 0)

#define vec_delete_at(v, i)                                                    \
  do {                                                                         \
    if ((i) < (v).size) {                                                      \
      memmove(&(v).data[i], &(v).data[i + 1],                                  \
              ((v).size - (i) - 1) * sizeof(*(v).data));                       \
      (v).size--;                                                              \
    }                                                                          \
  } while (0)

#define vec_get(v, i) ((v).data[(i)])

#define vec_set(v, i, val)                                                     \
  do {                                                                         \
    if ((i) < (v).size)                                                        \
      (v).data[(i)] = (val);                                                   \
  } while (0)

#define vec_resize(v, n)                                                       \
  do {                                                                         \
    if ((n) > (v).cap)                                                         \
      vec_reserve(v, (n));                                                     \
    if ((n) > (v).size) {                                                      \
      memset(&(v).data[(v).size], 0, ((n) - (v).size) * sizeof(*(v).data));    \
    }                                                                          \
    (v).size = (n);                                                            \
  } while (0)

#define vec_foreach(type, v, it)                                               \
  for (type *it = (v).data; it != (v).data + (v).size; ++it)

// move items from vector into array on arena
#define vec_move_into_arena(arena_ptr, v, type, arr)                           \
  do {                                                                         \
    arr = ARENA_ALLOC_ARRAY(arena_ptr, type, v.size);                          \
    memcpy(arr, v.data, sizeof(type) * v.size);                                \
  } while (0)
