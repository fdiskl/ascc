#ifndef _ASCC_PARSER_H
#define _ASCC_PARSER_H

#include "arena.h"
#include "scan.h"
#include "strings.h"
#include "table.h"
#include "vec.h"
#include <stdint.h>

typedef struct _decl decl;
typedef struct _stmt stmt;
typedef struct _expr expr;

typedef decl program;

typedef struct _parser parser;

typedef struct _block_stmt block_stmt;
typedef struct _ast_pos ast_pos;

typedef struct _block_item block_item;

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
typedef struct _assignment assignment;
typedef struct _var_expr var_expr;

typedef enum {
  EXPR_INT_CONST,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_ASSIGNMENT,
  EXPR_VAR
} exprt;

struct _int_const {
  uint64_t v;
};

typedef enum {
  UNARY_NEGATE,
  UNARY_COMPLEMENT,
  UNARY_NOT,
  UNARY_PREFIX_INC,
  UNARY_PREFIX_DEC,
  UNARY_POSTFIX_INC,
  UNARY_POSTFIX_DEC,
} unaryt;

struct _unary {
  unaryt t;
  expr *e;
};

typedef enum {
  BINARY_ADD = 1, // important to be > 0
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

typedef enum {
  ASSIGN = 1, // important to be > 0

  ASSIGN_ADD,
  ASSIGN_SUB,
  ASSIGN_MUL,
  ASSIGN_DIV,
  ASSIGN_MOD,
  ASSIGN_AND,
  ASSIGN_OR,
  ASSIGN_XOR,
  ASSIGN_LSHIFT,
  ASSIGN_RSHIFT,
} assignment_t;

struct _binary {
  binaryt t;
  expr *l;
  expr *r;
};

// in theory could be merged with binary expr, but as separatr struct provides
// more clarity
struct _assignment {
  assignment_t t;
  expr *l;
  expr *r;
};

struct _var_expr {
  string name;  // name in src
  int name_idx; // idx representing name
};

struct _expr {
  exprt t;
  ast_pos pos;
  union {
    int_const intc;
    unary u;
    binary b;
    assignment assignment;
    var_expr var;
  } v;
};

/*
 *
 * STMTS
 *
 */

typedef struct _return_stmt return_stmt;

typedef enum {
  STMT_RETURN,
  STMT_BLOCK,
  STMT_EXPR,
  STMT_NULL,
} stmtt;

struct _return_stmt {
  expr *e;
};

struct _block_stmt {
  block_item *items;
  size_t items_len;
};

struct _stmt {
  stmtt t;
  ast_pos pos;
  union {
    return_stmt ret;
    block_stmt block;
    expr *e;
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
  string name;  // name in src
  int name_idx; // unique idx representing name
  expr *init;   // NULL if not present
};

struct _func_decl {
  string name;
  block_item *body;
  size_t body_len;
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

struct _block_item {
  decl *d;
  stmt *s;
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
  arena bi_arena;

  ht *ident_ht_list_head;
};

void init_parser(parser *p, lexer *l);
program *parse(parser *p);

void print_program(program *p);

#endif
