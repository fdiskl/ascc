#ifndef _ASCC_PARSER_H
#define _ASCC_PARSER_H

#include "scan.h"
#include "strings.h"
#include "vec.h"
#include <stdint.h>

typedef struct _decl decl;
typedef struct _stmt stmt;
typedef struct _expr expr;

typedef decl program;

typedef struct _parser parser;

typedef struct _block_stmt block_stmt;
typedef struct _ast_pos ast_pos;

struct _ast_pos {
  string filename;
  int line_start;
  int line_end;
  int pos_start;
  int pos_end;
};

/*
 *
 * EXPRS
 *
 */

typedef struct _int_const int_const;
typedef struct _unary unary;
typedef struct _binary binary;

typedef enum {
  EXPR_INT_CONST,
  EXPR_UNARY,
  EXPR_BINARY,
} exprt;

struct _int_const {
  uint64_t v;
};

typedef enum {
  UNARY_NEGATE,
  UNARY_COMPLEMENT,
  UNARY_NOT,
} unaryt;

struct _unary {
  unaryt t;
  expr *e;
};

typedef enum {
  BINARY_ADD,
  BINARY_SUB,
  BINARY_MUL,
  BINARY_DIV,
  BINARY_MOD,

  BINARY_BITWISE_AND,
  BINARY_BITWISE_OR,
  BINARY_XOR,
  BINARY_LSHIFT,
  BINARY_RSHIFT,

  BINARY_AND,
  BINARY_OR,
  BINARY_EQ,
  BINARY_NE,
  BINARY_LT,
  BINARY_GT,
  BINARY_LE,
  BINARY_GE,
} binaryt;

struct _binary {
  binaryt t;
  expr *l;
  expr *r;
};

struct _expr {
  exprt t;
  ast_pos pos;
  union {
    int_const intc;
    unary u;
    binary b;
  } v;
};

/*
 *
 * STMTS
 *
 */

typedef struct _return_stmt return_stmt;

typedef enum { STMT_RETURN, STMT_BLOCK } stmtt;

struct _return_stmt {
  expr *e;
};

struct _block_stmt {
  VEC(stmt *) stmts;
};

struct _stmt {
  stmtt t;
  ast_pos pos;
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

typedef enum {
  DECL_FUNC,
  DECL_VAR,
} declt;

struct _var_decl {
  // todo
};

struct _func_decl {
  string name;
  VEC(stmt *) body; // TODO: vec instead of fixed size array, ok for now
  // todo: return type, arg type
};

struct _decl {
  declt t;
  ast_pos pos;
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
