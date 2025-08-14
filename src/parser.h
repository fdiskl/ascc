#ifndef _ASCC_PARSER_H
#define _ASCC_PARSER_H

#include "scan.h"
#include "strings.h"
#include <stdint.h>

typedef struct _decl decl;
typedef struct _stmt stmt;
typedef struct _expr expr;

typedef decl program;

typedef struct _parser parser;

typedef struct _block_stmt block_stmt;

/*
 *
 * EXPRS
 *
 */

typedef struct _int_const int_const;

enum {
  EXPR_INT_CONST,
};

struct _int_const {
  uint64_t v;
};

struct _expr {
  int t;
  union {
    int_const intc;
  };
};

/*
 *
 * STMTS
 *
 */

typedef struct _return_stmt return_stmt;

enum { STMT_RETURN, STMT_BLOCK };

struct _return_stmt {
  expr *e;
};

struct _block_stmt {
  stmt *stmts[4]; // TODO: vec instead of fixed size array, ok for now
};

struct _stmt {
  int t;
  union {
    return_stmt ret;
    block_stmt block;
  } v;
};

/*
 *
 * DECLS
 *
 */

typedef struct _var_decl var_decl;
typedef struct _func_decl func_decl;

enum {
  DECL_FUNC,
  DECL_VAR,
};

struct _var_decl {
  // todo
};

struct _func_decl {
  string name;
  stmt *body[4]; // TODO: vec instead of fixed size array, ok for now
  // todo: return type, arg type
};

struct _decl {
  int t;
  union {
    func_decl func;
    var_decl var;
  } v;
  decl *next; // NULL if decl is not global
};

/*
 *
 * PARSER and it's FUNCS
 *
 */

struct _parser {
  lexer *l;

  token curr;
  token next;

  arena decl_arena;
  arena stmt_arena;
  arena expr_arena;
};

void init_parser(parser *p, lexer *l);
program *parse(parser *p);

void print_program(program *p);

#endif
