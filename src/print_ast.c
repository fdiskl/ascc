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
    printf("IntConst (%llu)", (unsigned long long)e->v.intc.v);
    print_ast_pos(e->pos);
    printf("\n");
    return;

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
    case UNARY_PREFIX_INC:
      printf("prefix inc");
      break;
    case UNARY_PREFIX_DEC:
      printf("prefix dec");
      break;
    case UNARY_POSTFIX_INC:
      printf("postfix inc");
      break;
    case UNARY_POSTFIX_DEC:
      printf("postfix dec");
      break;
    }
    printf(")");
    print_ast_pos(e->pos);
    printf("\n");
    print_expr(e->v.u.e, indent + 1);
    return;

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
    return;
  case EXPR_ASSIGNMENT:
    printf("Assignment (");
    switch (e->v.assignment.t) {
    case ASSIGN:
      printf("assign");
      break;
    case ASSIGN_ADD:
      printf("add");
      break;
    case ASSIGN_SUB:
      printf("sub");
      break;
    case ASSIGN_MUL:
      printf("mul");
      break;
    case ASSIGN_DIV:
      printf("div");
      break;
    case ASSIGN_MOD:
      printf("mod");
      break;
    case ASSIGN_AND:
      printf("and");
      break;
    case ASSIGN_OR:
      printf("or");
      break;
    case ASSIGN_XOR:
      printf("xor");
      break;
    case ASSIGN_LSHIFT:
      printf("lshift");
      break;
    case ASSIGN_RSHIFT:
      printf("rshift");
      break;
    }
    printf(")");
    print_ast_pos(e->pos);
    printf("\n");
    print_expr(e->v.assignment.l, indent + 1);
    print_expr(e->v.assignment.r, indent + 1);
    return;
  case EXPR_VAR:
    printf("Var(%s, %d)", e->v.var.name, e->v.var.name_idx);
    print_ast_pos(e->pos);
    printf("\n");
    return;
  case EXPR_TERNARY:
    printf("TernaryExpr");
    print_ast_pos(e->pos);
    printf("\n");
    print_expr(e->v.ternary.cond, indent + 1);
    print_expr(e->v.ternary.then, indent + 1);
    print_expr(e->v.ternary.elze, indent + 1);
    return;
  case EXPR_FUNC_CALL:
    printf("FuncCallExpr (%s)", e->v.func_call.name);
    print_ast_pos(e->pos);
    printf("\n");
    if (e->v.func_call.args != NULL)
      for (int i = 0; i < e->v.func_call.args_len; ++i)
        print_expr(e->v.func_call.args[i], indent + 1);
    else {
      print_indent(indent + 1);
      printf("(no args)\n");
    }
    break;
  }
  UNREACHABLE();
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
    for (int i = 0; i < s->v.block.items_len; ++i)
      print_bi(&s->v.block.items[i], indent + 1);
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
  case STMT_IF:
    printf("IfStmt");
    print_ast_pos(s->pos);
    printf("\n");
    print_expr(s->v.if_stmt.cond, indent + 1);
    print_stmt(s->v.if_stmt.then, indent + 1);
    if (s->v.if_stmt.elze != NULL)
      print_stmt(s->v.if_stmt.elze, indent + 1);
    else {
      print_indent(indent + 1);
      printf("(no else stmt)\n");
    }
    return;
  case STMT_GOTO:
    printf("GotoStmt (%s, %d)", s->v.goto_stmt.label, s->v.goto_stmt.label_idx);
    print_ast_pos(s->pos);
    printf("\n");
    return;
  case STMT_LABEL:
    printf("LabelStmt (%s, %d)", s->v.label.label, s->v.label.label_idx);
    print_ast_pos(s->pos);
    printf("\n");
    print_stmt(s->v.label.s, indent + 1);
    return;
  case STMT_WHILE:
    printf("WhileStmt (break: %d, continue: %d)",
           s->v.dowhile_stmt.break_label_idx,
           s->v.dowhile_stmt.continue_label_idx);
    print_ast_pos(s->pos);
    printf("\n");
    if (s->v.while_stmt.cond != NULL)
      print_expr(s->v.while_stmt.cond, indent + 1);
    print_stmt(s->v.while_stmt.s, indent + 1);
    return;
  case STMT_DOWHILE:
    printf("DoWhileStmt (break: %d, continue: %d)",
           s->v.dowhile_stmt.break_label_idx,
           s->v.dowhile_stmt.continue_label_idx);
    print_ast_pos(s->pos);
    printf("\n");
    print_stmt(s->v.dowhile_stmt.s, indent + 1);
    if (s->v.dowhile_stmt.cond != NULL)
      print_expr(s->v.dowhile_stmt.cond, indent + 1);
    return;
  case STMT_FOR:
    printf("ForStmt (break: %d, continue: %d)", s->v.for_stmt.break_label_idx,
           s->v.for_stmt.continue_label_idx);
    print_ast_pos(s->pos);
    printf("\n");
    if (s->v.for_stmt.init_d != NULL)
      print_decl(s->v.for_stmt.init_d, indent + 1);
    else if (s->v.for_stmt.init_e != NULL)
      print_expr(s->v.for_stmt.init_e, indent + 1);
    else {
      print_indent(indent + 1);
      printf("(no init)\n");
    }
    if (s->v.for_stmt.cond)
      print_expr(s->v.for_stmt.cond, indent + 1);
    else {
      print_indent(indent + 1);
      printf("(no cond)\n");
    }
    if (s->v.for_stmt.post)
      print_expr(s->v.for_stmt.post, indent + 1);
    else {
      print_indent(indent + 1);
      printf("(no post)\n");
    }
    print_stmt(s->v.for_stmt.s, indent + 1);
    return;
  case STMT_BREAK:
    printf("BreakStmt (%d)", s->v.break_stmt.idx);
    print_ast_pos(s->pos);
    printf("\n");
    return;
  case STMT_CONTINUE:
    printf("ContinueStmt (%d)", s->v.continue_stmt.idx);
    print_ast_pos(s->pos);
    printf("\n");
    return;
  case STMT_CASE:
    printf("CaseStmt (%d)", s->v.case_stmt.label_idx);
    print_ast_pos(s->pos);
    printf("\n");
    print_expr(s->v.case_stmt.e, indent + 1);
    print_stmt(s->v.case_stmt.s, indent + 1);
    return;
  case STMT_DEFAULT:
    printf("DefaultStmt (%d)", s->v.default_stmt.label_idx);
    print_ast_pos(s->pos);
    printf("\n");
    print_stmt(s->v.default_stmt.s, indent + 1);
    return;
  case STMT_SWITCH:
    if (s->v.switch_stmt.default_stmt != NULL)
      printf("SwitchStmt (break: %d, default: %d)",
             s->v.switch_stmt.break_label_idx,
             s->v.switch_stmt.default_stmt->v.default_stmt.label_idx);
    else
      printf("SwitchStmt (break: %d, default: none)",
             s->v.switch_stmt.break_label_idx);
    print_ast_pos(s->pos);
    printf("\n");

    // print cases
    print_indent(indent + 1);
    printf("cases: ");
    if (s->v.switch_stmt.cases_len > 0) {
      stmt **arr = s->v.switch_stmt.cases;
      printf("%d", arr[0]->v.case_stmt.label_idx);
      for (int i = 1; i < s->v.switch_stmt.cases_len; ++i)
        printf(", %d", arr[i]->v.case_stmt.label_idx);

      printf("\n");
    }

    print_expr(s->v.switch_stmt.e, indent + 1);
    print_stmt(s->v.switch_stmt.s, indent + 1);
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
    if (d->v.func.params != NULL) {
      print_indent(indent + 1);
      printf("%s(%d)", d->v.func.params[0], d->v.func.params_idxs[0]);
      for (int i = 1; i < d->v.func.params_len; ++i)
        printf(", %s(%d)", d->v.func.params[i], d->v.func.params_idxs[i]);
    } else {
      print_indent(indent + 1);
      printf("(no params)\n");
    }
    if (d->v.func.body != NULL)
      for (int i = 0; i < d->v.func.body_len; ++i)
        print_bi(&d->v.func.body[i], indent + 1);
    else {
      print_indent(indent + 1);
      printf("(no body)\n");
    }
    break;

  case DECL_VAR:
    printf("VarDecl (%s, %d)", d->v.var.name, d->v.var.name_idx);
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
