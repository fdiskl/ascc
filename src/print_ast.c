#include "common.h"
#include "parser.h"
#include "vec.h"
#include <stdio.h>

static void print_indent(int indent) {
  for (int i = 0; i < indent; i++)
    printf("  ");
}

static void print_expr(expr *e, int indent) {
  if (!e) {
    print_indent(indent);
    printf("(null expr)\n");
    return;
  }

  print_indent(indent);
  switch (e->t) {
  case EXPR_INT_CONST:
    printf("IntConst %llu\n", (unsigned long long)e->intc.v);
    break;
  case EXPR_UNARY:
    printf("Unary (");
    switch (e->u.t) {
    case UNARY_NEGATE:
      printf("negate");
      break;
    case UNARY_COMPLEMENT:
      printf("complement");
      break;
    }
    printf(")\n");
    print_expr(e->u.e, indent + 1);
    break;
  default:
    unreachable();
  }
}

static void print_stmt(stmt *s, int indent) {
  if (!s) {
    print_indent(indent);
    printf("(null stmt)\n");
    return;
  }

  switch (s->t) {
  case STMT_RETURN:
    print_indent(indent);
    printf("ReturnStmt\n");
    print_expr(s->v.ret.e, indent + 1);
    break;

  case STMT_BLOCK:
    print_indent(indent);
    printf("BlockStmt\n");

    vec_foreach(stmt *, s->v.block.stmts, it) print_stmt(*it, indent + 1);

    break;
  default:
    unreachable();
  }
}

static void print_decl(decl *d, int indent) {
  if (!d)
    return;

  switch (d->t) {
  case DECL_FUNC:
    print_indent(indent);
    printf("FuncDecl (%s)\n", d->v.func.name);

    vec_foreach(stmt *, d->v.func.body, it) print_stmt(*it, indent + 1);

    break;

  case DECL_VAR:
    print_indent(indent);
    printf("VarDecl\n");
    break;

  default:
    unreachable();
  }

  if (d->next)
    print_decl(d->next, indent);
}

void print_program(program *p) { print_decl(p, 0); }
