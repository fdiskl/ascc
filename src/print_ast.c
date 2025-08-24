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
    case UNARY_NOT:
      printf("not");
      break;
    }
    printf(")");
    print_ast_pos(e->pos);
    printf("\n");
    print_expr(e->v.u.e, indent + 1);
    break;

  case EXPR_BINARY:
    printf("Binary (");
    switch (e->v.b.t) {
    case BINARY_ADD:
      printf("add");
      break;
    case BINARY_SUB:
      printf("sub");
      break;
    case BINARY_MUL:
      printf("mul");
      break;
    case BINARY_DIV:
      printf("div");
      break;
    case BINARY_MOD:
      printf("mod");
      break;
    case BINARY_BITWISE_AND:
      printf("bitwise and");
      break;
    case BINARY_BITWISE_OR:
      printf("bitwise or");
      break;
    case BINARY_XOR:
      printf("xor");
      break;
    case BINARY_LSHIFT:
      printf("lshift");
      break;
    case BINARY_RSHIFT:
      printf("rshift");
      break;
    case BINARY_AND:
      printf("and");
      break;
    case BINARY_OR:
      printf("or");
      break;
    case BINARY_EQ:
      printf("eq");
      break;
    case BINARY_NE:
      printf("ne");
      break;
    case BINARY_LT:
      printf("lt");
      break;
    case BINARY_GT:
      printf("gt");
      break;
    case BINARY_LE:
      printf("le");
      break;
    case BINARY_GE:
      printf("ge");
      break;
    }
    printf(")");
    print_ast_pos(e->pos);
    printf("\n");
    print_expr(e->v.b.l, indent + 1);
    print_expr(e->v.b.r, indent + 1);
    break;
  case EXPR_ASSIGNMENT:
    printf("Assignment");
    print_ast_pos(e->pos);
    printf("\n");
    print_expr(e->v.assignment.l, indent + 1);
    print_expr(e->v.assignment.r, indent + 1);
    break;
  case EXPR_VAR:
    printf("Var(%s)", e->v.var.name);
    print_ast_pos(e->pos);
    printf("\n");
    break;
  default:
    UNREACHABLE();
  }
}

static void print_decl(decl *d, int indent);
static void print_stmt(stmt *s, int indent);

static void print_bi(const block_item *bi, int indent) {
  if (bi->d != NULL)
    print_decl(bi->d, indent);
  else
    print_stmt(bi->s, indent);
}

static void print_stmt(stmt *s, int indent) {
  print_indent(indent);

  switch (s->t) {
  case STMT_RETURN:
    printf("ReturnStmt");
    print_ast_pos(s->pos);
    printf("\n");
    print_expr(s->v.ret.e, indent + 1);
    return;

  case STMT_BLOCK:
    printf("BlockStmt");
    print_ast_pos(s->pos);
    printf("\n");
    vec_foreach(block_item, s->v.block.items, it) print_bi(it, indent + 1);
    return;
  case STMT_EXPR:
    printf("ExprStmt");
    print_ast_pos(s->pos);
    printf("\n");
    print_expr(s->v.e, indent + 1);
    return;
  case STMT_NULL:
    printf("NullStmt");
    print_ast_pos(s->pos);
    printf("\n");
    print_expr(s->v.e, indent + 1);
    return;
  }

  UNREACHABLE();
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
    vec_foreach(block_item, d->v.func.body, it) print_bi(it, indent + 1);
    break;

  case DECL_VAR:
    printf("VarDecl (%s)", d->v.var.name);
    print_ast_pos(d->pos);
    printf("\n");
    if (d->v.var.init != NULL)
      print_expr(d->v.var.init, indent + 1);
    break;

  default:
    UNREACHABLE();
  }

  if (d->next)
    print_decl(d->next, indent);
}

void print_program(program *p) { print_decl(p, 0); }
