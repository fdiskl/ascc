#ifndef _ASCC_TYPECHECK_H

#include "parser.h"

typedef struct _sym_table_entry syme;
typedef ht *sym_table;

typedef struct _type type;

typedef enum {
  TYPE_INT,
  TYPE_FN,
} typet;

struct _fn_type {
  int param_count;
  char defined;
};

struct _type {
  typet t;
  union {
    struct _fn_type fntype;
  } v;
};

struct _sym_table_entry {
  string original_name;
  int name_idx;
  decl *ref; // can be NULL too
  type *t;
};

sym_table typecheck(program *p);
void print_sym_table(sym_table st);

#endif
