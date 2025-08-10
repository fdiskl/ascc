#include "string.h"
#include "arena.h"
#include <assert.h>
#include <string.h>

arena str_arena;

string new_string(const char *s) {
  unsigned long len = strlen(s);
  char *dst = ARENA_ALLOC_ARRAY(&str_arena, char, len + 1);
  assert(dst);
  memcpy(dst, s, len);
  dst[len] = '\0';
  return dst;
}
