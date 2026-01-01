#ifndef _ASCC_TYPECHECK_H
#define _ASCC_TYPECHECK_H

#include "parser.h"
#include "type.h"
#include <stdint.h>

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

typedef struct _initial_init initial_init;

typedef enum {
  INITIAL_INT,
  INITIAL_LONG,
  INITIAL_UINT,
  INITIAL_ULONG,
} inital_init_t;

struct _initial_init {
  inital_init_t t;
  uint64_t v;
};

struct _init_value {
  init_value_t t;
  initial_init v;
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
void label_loop(program *p); // loop-labeling.c

initial_init const_to_initial(int_const original); // convert_intc.c
int_const convert_const(int_const original,
                        type *convert_to); // convert_intc.c
#endif
