#include "common.h"
#include "parser.h"
#include "vec.h"
#include <stdio.h>

static void print_ast_pos(ast_pos p) {
#ifdef AST_PRINT_LOC
#ifdef AST_PRINT_FILENAME_LOC
  printf("\t[%s:%d:%d-%d:%d]", p.filename, p.line_start, p.pos_start,
         p.line_end, p.pos_end);
#else
  printf("\t[%d:%d-%d:%d]", p.line_start, p.pos_start, p.line_end, p.pos_end);
#endif
#endif
}

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
    printf("IntConst %llu", (unsigned long long)e->v.intc.v);
    print_ast_pos(e->pos);
    printf("\n");
    break;

  case EXPR_UNARY:
    printf("Unary (");
    switch (e->v.u.t) {
    case UNARY_NEGATE:
      printf("negate");
      break;
    case UNARY_COMPLEMENT:
      printf("complement");
      break;
    }
    printf(")");
    print_ast_pos(e->pos);
    printf("\n");
    print_expr(e->v.u.e, indent + 1);
    break;

  default:
    unreachable();
  }
}

static void print_stmt(stmt *s, int indent) {
  print_indent(indent);

  switch (s->t) {
  case STMT_RETURN:
    printf("ReturnStmt");
    print_ast_pos(s->pos);
    printf("\n");
    print_expr(s->v.ret.e, indent + 1);
    break;

  case STMT_BLOCK:
    printf("BlockStmt");
    print_ast_pos(s->pos);
    printf("\n");
    vec_foreach(stmt *, s->v.block.stmts, it) print_stmt(*it, indent + 1);
    break;

  default:
    unreachable();
  }
}

static void print_decl(decl *d, int indent) {
  if (!d)
    return;

  print_indent(indent);
  switch (d->t) {
  case DECL_FUNC:
    printf("FuncDecl (%s)", d->v.func.name);
    print_ast_pos(d->pos);
    printf("\n");
    vec_foreach(stmt *, d->v.func.body, it) print_stmt(*it, indent + 1);
    break;

  case DECL_VAR:
    printf("VarDecl");
    print_ast_pos(d->pos);
    printf("\n");
    break;

  default:
    unreachable();
  }

  if (d->next)
    print_decl(d->next, indent);
}

void print_program(program *p) { print_decl(p, 0); }
