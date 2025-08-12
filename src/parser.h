#ifndef _ASCC_PARSER_H
#define _ASCC_PARSER_H

#include "scan.h"
#include "strings.h"

typedef struct _decl decl;
typedef struct _stmt stmt;
typedef struct _expr expr;

typedef decl program;

typedef struct _parser parser;

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
 * STMTS
 *
 */

typedef struct _return_stmt return_stmt;

enum { STMT_RETURN };

struct _return_stmt {
  expr *e;
};

struct _stmt {
  int t;
  union {
    return_stmt ret;
  } v;
};

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
  int v;
};

struct _expr {
  int t;
  union {
    int_const intc;
  };
};

/*
 *
 * PARSER and it's FUNCS
 *
 */

struct _parser {
  lexer *l;
  // todo
};

void init_parser(parser *p, lexer *l);
program *parse(parser *p);

#endif
