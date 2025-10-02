#ifndef _ASCC_TYPECHECK_H
#define _ASCC_TYPECHECK_H

#include "parser.h"
#include "type.h"

typedef struct _sym_table_entry syme;
typedef struct _sym_table sym_table;

typedef struct _sym_table_attrs attrs;

typedef enum {
  ATTR_FUNC,
  ATTR_STATIC,
  ATTR_LOCAL,
} attrt;

struct _attr_func {
  char defined;
  char global;
};

typedef enum {
  INIT_TENTATIVE,
  INIT_INITIAL,
  INIT_NOINIT,
} init_value_t;

struct _init_value {
  init_value_t t;
  uint64_t v;
};

struct _attr_static {
  struct _init_value init;
  char global;
};

struct _sym_table_attrs {
  attrt t;
  union {
    struct _attr_func f;
    struct _attr_static s;
  } v;
};

struct _sym_table_entry {
  string original_name;
  string name;
  decl *ref; // can be NULL too
  type *t;
  attrs a;
};

struct _sym_table {
  ht *t;
  arena *entry_arena; // will be freed by free_sym_table
};

sym_table typecheck(program *p);
void print_sym_table(sym_table *st);
void free_sym_table(sym_table *st);

#endif
