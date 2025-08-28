#include "strings.h"
#include "arena.h"
#include "string.h"
#include <assert.h>
#include <stdarg.h>

arena str_arena;

string new_string(const char *s) {
  unsigned long len = strlen(s);
  char *dst = ARENA_ALLOC_ARRAY(&str_arena, char, len + 1);
  assert(dst);
  memcpy(dst, s, len);
  dst[len] = '\0';
  return dst;
}

string string_sprintf(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  int len = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  if (len < 0) {
    return NULL;
  }

  string s = ARENA_ALLOC_ARRAY(&str_arena, char, len + 1);

  va_start(args, fmt);
  vsnprintf(s, len + 1, fmt, args);
  va_end(args);

  return s;
}
