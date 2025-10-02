#ifndef _ASCC_TYPE_H
#define _ASCC_TYPE_H

#include "arena.h"

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

const char *type_name(type *t); // defined in typecheck.c
type *new_type(int t);          // defined in typecheck.c

#endif
