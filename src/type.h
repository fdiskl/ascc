#ifndef _ASCC_TYPE_H
#define _ASCC_TYPE_H

#include "arena.h"
#include "strings.h"

extern arena *types_arena;

typedef struct _type type;

typedef enum {
  TYPE_INT,
  TYPE_LONG,
  TYPE_FN,
} typet;

struct _fn_type {
  type *return_type;
  type **params;
  int param_count;
};

struct _type {
  typet t;
  union {
    struct _fn_type fntype;
  } v;
};

#define EMIT_TYPE_INTO_BUF(buf_name, buf_len, type)                            \
  static char buf_name[buf_len];                                               \
  {                                                                            \
    size_t pos = 0;                                                            \
    emit_type_name_buf(buf_name, buf_len, &pos, type);                         \
  }

void emit_type_name_buf(char *buf, size_t size, size_t *pos, type *t);

// will allocate string for type name, should be used if names will be stored
// long-term
string *emit_type_name_str(type *t);

type *new_type(int t); // defined in typecheck.c

bool types_eq(type *t1, type *t2);

#endif
