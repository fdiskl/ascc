#include "common.h"
#include "parser.h"
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

  switch (e->t) {
  case EXPR_INT_CONST:
    print_indent(indent);
    printf("IntConst %llu\n", (unsigned long long)e->intc.v);
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
    for (int i = 0; i < 4; i++) {
      if (s->v.block.stmts[i])
        print_stmt(s->v.block.stmts[i], indent + 1);
    }
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
    for (int i = 0; i < 4;
         i++) // TODO: change when array is replaced with vector
      if (d->v.func.body[i] != NULL)
        print_stmt(d->v.func.body[i], indent + 1);
      else
        break;

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
