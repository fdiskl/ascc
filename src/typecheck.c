#include "typecheck.h"
#include "arena.h"
#include "common.h"
#include "parser.h"
#include "table.h"
#include "type.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct _checker checker;

struct _checker {
  arena *syme_arena;
  arena *expr_arena; // pulled from ast program, is not managed by checker
  decl *curr_func;
  ht *st;
};

type *new_type(int t) {
  type *res = ARENA_ALLOC_OBJ(types_arena, type);
  res->t = t;
  return res;
}

static syme *new_syme(checker *c, type *t, string name, decl *origin, attrs a,
                      string original_name) {
  syme *e = ARENA_ALLOC_OBJ(c->syme_arena, syme);
  e->name = name;
  e->original_name = original_name;
  e->ref = origin;
  e->t = t;
  e->a = a;
  return e;
}

static syme *add_to_symtable(checker *c, type *t, string name, decl *origin,
                             attrs a, string original_name) {
  syme *e;
  ht_set(c->st, name, e = new_syme(c, t, name, origin, a, original_name));

  return e;
}

bool types_eq(type *t1, type *t2) {

  if (t1->t != t2->t)
    return false;

  // check funcs
  if (t1->t == TYPE_FN) {
    if (t1->v.fntype.param_count != t2->v.fntype.param_count)
      return false;
    if (!types_eq(t1->v.fntype.return_type, t2->v.fntype.return_type))
      return false;

    for (int i = 0; i < t1->v.fntype.param_count; ++i) {
      if (!types_eq(t1->v.fntype.params[i], t2->v.fntype.params[i]))
        return false;
    }
  }

  return true;
}

static void typecheck_var_expr(checker *c, expr *e) {
  syme *entry = ht_get(c->st, e->v.var.name);
  if (entry->t->t == TYPE_FN) {
    fprintf(stderr, "function name used as variable %s, (%d:%d-%d:%d)\n",
            e->v.var.name, e->pos.line_start, e->pos.pos_start, e->pos.line_end,
            e->pos.pos_end);

    exit(1);
  }

  e->tp = entry->t;
}

static type *get_common_type(type *t1, type *t2) {
  assert(t1->t != TYPE_FN && t2->t != TYPE_FN);

  if (types_eq(t1, t2))
    return new_type(t1->t);
  if (type_rank(t1) == type_rank(t2)) {
    if (type_signed(t1))
      return t2;
    else
      return t1;
  }

  if (type_rank(t1) > type_rank(t2))
    return t1;
  else
    return t2;
}

static expr *convert_to(checker *c, expr *e, type *t) {
  if (types_eq(e->tp, t))
    return e;

  expr *res = ARENA_ALLOC_OBJ(c->expr_arena, expr);
  res->t = EXPR_CAST;
  res->v.cast.tp = res->tp = t;
  res->pos = e->pos;
  res->v.cast.e = e;

  return res;
}

static void typecheck_expr(checker *c, expr *e);

static void typecheck_fn_call_expr(checker *c, expr *e) {
  syme *entry = ht_get(c->st, e->v.var.name);
  if (entry->t->t != TYPE_FN) {
    fprintf(stderr, "variable used as function %s (%d:%d-%d:%d)\n",
            entry->original_name, e->pos.line_start, e->pos.pos_start,
            e->pos.line_end, e->pos.pos_end);

    exit(1);
  }

  if (entry->t->v.fntype.param_count != e->v.func_call.args_len) {
    fprintf(stderr,
            "invalid arg count when calling %s, expected %d, got %zu, "
            "(%d:%d-%d:%d)\n",
            e->v.func_call.name, entry->t->v.fntype.param_count,
            e->v.func_call.args_len, e->pos.line_start, e->pos.pos_start,
            e->pos.line_end, e->pos.pos_end);

    exit(1);
  }

  for (int i = 0; i < e->v.func_call.args_len; ++i) {
    typecheck_expr(c, e->v.func_call.args[i]);
    e->v.func_call.args[i] =
        convert_to(c, e->v.func_call.args[i], entry->t->v.fntype.params[i]);
  }

  e->tp = entry->t->v.fntype.return_type;
}

static void typecheck_const_expr(checker *c, expr *e) {
  switch (e->v.intc.t) {
  case CONST_INT:
    e->tp = new_type(TYPE_INT);
    return;

  case CONST_UINT:
    e->tp = new_type(TYPE_UINT);
    return;

  case CONST_LONG:
    e->tp = new_type(TYPE_LONG);
    return;

  case CONST_ULONG:
    e->tp = new_type(TYPE_ULONG);
    return;

  default:
    UNREACHABLE();
  }
}

static void typecheck_cast_expr(checker *c, expr *e) {
  typecheck_expr(c, e->v.cast.e);
  e->tp = e->v.cast.tp;
}

static void typecheck_unary_expr(checker *c, expr *e) {
  typecheck_expr(c, e->v.u.e);
  switch (e->v.u.t) {
  case UNARY_NOT:
    e->tp = new_type(TYPE_INT);
    return;
  default:
    e->tp = e->v.u.e->tp;
    return;
  }
}

static void typecheck_binary_expr(checker *c, expr *e) {
  typecheck_expr(c, e->v.b.l);
  typecheck_expr(c, e->v.b.r);

  binaryt bt = e->v.b.t;

  if (bt == BINARY_OR || bt == BINARY_AND) {
    e->tp = new_type(TYPE_INT);
    return;
  }

  switch (e->v.b.t) {
  case BINARY_ADD:
  case BINARY_SUB:
  case BINARY_MUL:
  case BINARY_DIV:
  case BINARY_MOD:
  case BINARY_BITWISE_AND:
  case BINARY_BITWISE_OR:
  case BINARY_XOR: {
    type *common = get_common_type(e->v.b.l->tp, e->v.b.r->tp);
    e->v.b.l = convert_to(c, e->v.b.l, common);
    e->v.b.r = convert_to(c, e->v.b.r, common);
    e->tp = common;
    return;
  }
  case BINARY_LSHIFT:
  case BINARY_RSHIFT:
    e->v.b.l = convert_to(c, e->v.b.l, e->v.b.l->tp);
    e->v.b.r = convert_to(c, e->v.b.r, e->v.b.r->tp);
    e->tp = e->v.b.l->tp;
    /*
    The integer promotions are performed on each of the operands. The type of
    the result is that of the promoted left operand.

    (6.5.7/3 of C99)
     */
    return;
  case BINARY_EQ:
  case BINARY_NE:
  case BINARY_LT:
  case BINARY_GT:
  case BINARY_LE:
  case BINARY_GE: {
    type *common = get_common_type(e->v.b.l->tp, e->v.b.r->tp);
    e->v.b.l = convert_to(c, e->v.b.l, common);
    e->v.b.r = convert_to(c, e->v.b.r, common);

    e->tp = new_type(TYPE_INT);
    return;
  }
  case BINARY_AND:
  case BINARY_OR:
    UNREACHABLE();
  }
}

static void typecheck_assignment_expr(checker *c, expr *e) {
  typecheck_expr(c, e->v.assignment.l);
  typecheck_expr(c, e->v.assignment.r);
  e->tp = e->v.assignment.l->tp;

  if (e->v.assignment.t == ASSIGN ||
      types_eq(e->v.assignment.l->tp, e->v.assignment.r->tp)) {
    e->v.assignment.r = convert_to(c, e->v.assignment.r, e->v.assignment.l->tp);
    return;
  }

  // convert assign with op into assign and binary
  binaryt op;
  bool is_shift = false;
  switch (e->v.assignment.t) {
  case ASSIGN:
    UNREACHABLE();
  case ASSIGN_ADD:
    op = BINARY_ADD;
    break;
  case ASSIGN_SUB:
    op = BINARY_SUB;
    break;
  case ASSIGN_MUL:
    op = BINARY_MUL;
    break;
  case ASSIGN_DIV:
    op = BINARY_DIV;
    break;
  case ASSIGN_MOD:
    op = BINARY_MOD;
    break;
  case ASSIGN_AND:
    op = BINARY_BITWISE_AND;
    break;
  case ASSIGN_OR:
    op = BINARY_BITWISE_OR;
    break;
  case ASSIGN_XOR:
    op = BINARY_XOR;
    break;
  case ASSIGN_LSHIFT:
    op = BINARY_LSHIFT;
    is_shift = true;
    break;
  case ASSIGN_RSHIFT:
    op = BINARY_RSHIFT;
    is_shift = true;
    break;
  }

  /*
  For shifts:

  The integer promotions are performed on each of the operands. The type of
  the result is that of the promoted left operand.

  (6.5.7/3 of C99)
   */

  type *common =
      is_shift ? e->v.assignment.l->tp
               : get_common_type(e->v.assignment.l->tp, e->v.assignment.r->tp);

  expr *b_expr = ARENA_ALLOC_OBJ(c->expr_arena, expr);
  b_expr->t = EXPR_BINARY;
  b_expr->tp = common;
  b_expr->v.b.l = convert_to(c, e->v.assignment.l, common);
  b_expr->v.b.r = convert_to(c, e->v.assignment.r, common);
  b_expr->v.b.t = op;

  e->v.assignment.t = ASSIGN;
  e->v.assignment.r = convert_to(c, b_expr, e->v.assignment.l->tp);
}

static void typecheck_ternary_expr(checker *c, expr *e) {
  typecheck_expr(c, e->v.ternary.cond);
  typecheck_expr(c, e->v.ternary.then);
  typecheck_expr(c, e->v.ternary.elze);

  e->tp = get_common_type(e->v.ternary.then->tp, e->v.ternary.elze->tp);

  e->v.ternary.then = convert_to(c, e->v.ternary.then, e->tp);
  e->v.ternary.elze = convert_to(c, e->v.ternary.elze, e->tp);
}

static void typecheck_expr(checker *c, expr *e) {
  switch (e->t) {
  case EXPR_INT_CONST:
    typecheck_const_expr(c, e);
    break;
  case EXPR_UNARY:
    typecheck_unary_expr(c, e);
    break;
  case EXPR_BINARY:
    typecheck_binary_expr(c, e);
    break;
  case EXPR_ASSIGNMENT:
    typecheck_assignment_expr(c, e);
    break;
  case EXPR_TERNARY:
    typecheck_ternary_expr(c, e);
    break;
  case EXPR_VAR:
    typecheck_var_expr(c, e);
    break;
  case EXPR_FUNC_CALL:
    typecheck_fn_call_expr(c, e);
    break;
  case EXPR_CAST:
    typecheck_cast_expr(c, e);
    break;
  }
}

static void typecheck_decl(checker *c, decl *d);
static void typecheck_stmt(checker *c, stmt *s);

static void typecheck_block(checker *c, block_item *arr, size_t len) {
  for (int i = 0; i < len; ++i) {
    if (arr[i].d != NULL)
      typecheck_decl(c, arr[i].d);
    else
      typecheck_stmt(c, arr[i].s);
  }
}

static void typecheck_return_stmt(checker *c, stmt *s) {
  typecheck_expr(c, s->v.ret.e);
  assert(c->curr_func != NULL);
  assert(c->curr_func->tp->t == TYPE_FN);
  s->v.ret.e =
      convert_to(c, s->v.ret.e, c->curr_func->tp->v.fntype.return_type);
}

static void typecheck_stmt(checker *c, stmt *s) {
  switch (s->t) {
  case STMT_RETURN:
    typecheck_return_stmt(c, s);
    break;
  case STMT_BLOCK:
    typecheck_block(c, s->v.block.items, s->v.block.items_len);
    break;
  case STMT_EXPR:
    typecheck_expr(c, s->v.e);
    break;

  case STMT_IF:
    typecheck_expr(c, s->v.if_stmt.cond);
    typecheck_stmt(c, s->v.if_stmt.then);
    if (s->v.if_stmt.elze != NULL)
      typecheck_stmt(c, s->v.if_stmt.elze);
    break;
  case STMT_WHILE:
    typecheck_expr(c, s->v.while_stmt.cond);
    typecheck_stmt(c, s->v.while_stmt.s);
    break;
  case STMT_DOWHILE:
    typecheck_expr(c, s->v.dowhile_stmt.cond);
    typecheck_stmt(c, s->v.dowhile_stmt.s);
    break;
  case STMT_FOR:
    if (s->v.for_stmt.init_d) {
      if (s->v.for_stmt.init_d->sc != SC_NONE) {
        ast_pos pos = s->v.for_stmt.init_d->pos;
        fprintf(stderr,
                "Can't use storage class in for loop init declaration "
                "%d:%d-%d:%d\n",
                pos.line_start, pos.pos_start, pos.line_end, pos.pos_end);
        exit(1);
      }
      typecheck_decl(c, s->v.for_stmt.init_d);
    } else if (s->v.for_stmt.init_e)
      typecheck_expr(c, s->v.for_stmt.init_e);
    if (s->v.for_stmt.cond)
      typecheck_expr(c, s->v.for_stmt.cond);
    if (s->v.for_stmt.post)
      typecheck_expr(c, s->v.for_stmt.post);
    typecheck_stmt(c, s->v.for_stmt.s);
    break;
  case STMT_CASE:
    // no need to typecheck const exprs
    typecheck_stmt(c, s->v.case_stmt.s);
    break;
  case STMT_SWITCH:
    typecheck_expr(c, s->v.switch_stmt.e);
    typecheck_stmt(c, s->v.switch_stmt.s);
    break;
  case STMT_LABEL:
    typecheck_stmt(c, s->v.label.s);
    break;
  case STMT_DEFAULT:
    typecheck_stmt(c, s->v.default_stmt.s);
    break;
  case STMT_BREAK:
  case STMT_NULL:
  case STMT_GOTO:
  case STMT_CONTINUE:
    break;
  }
}

static void init_checker(checker *c, arena *e_arena) {
  NEW_ARENA(c->syme_arena, syme);
  c->expr_arena = e_arena;

  c->st = ht_create();
}

static void typecheck_func_decl(checker *c, decl *d) {
  type *t = d->tp;
  char has_body = d->v.func.bs != NULL ? 1 : 0;
  char alr_defined = false;
  char global = d->sc != SC_STATIC;

  syme *e = ht_get(c->st, d->v.var.name);

  if (d->sc == SC_STATIC && d->scope != 0) {
    ast_pos curr = d->pos;
    fprintf(stderr,
            "function %s is declared in block, but has static storage class"
            "(%d:%d-%d:%d)\n",
            d->v.func.name, curr.line_start, curr.pos_start, curr.line_end,
            curr.pos_end);
    exit(1);
  }

  if (e != NULL) {
    if (!types_eq(e->t, t)) {
      ast_pos old = e->ref->pos;
      ast_pos curr = d->pos;
      fprintf(stderr,
              "function %s has declaration with incompatible types "
              "(%d:%d-%d:%d), other decl at %d:%d-%d:%d\n",
              d->v.func.name, curr.line_start, curr.pos_start, curr.line_end,
              curr.pos_end, old.line_start, old.pos_start, old.line_end,
              old.pos_end);
      exit(1);
    }
    assert(e->a.t == ATTR_FUNC);
    alr_defined = e->a.v.f.defined;
    if (alr_defined && has_body) {
      ast_pos old = e->ref->pos;
      ast_pos curr = d->pos;
      fprintf(stderr,
              "function %s is defined more then once "
              "(%d:%d-%d:%d), other decl at %d:%d-%d:%d\n",
              d->v.func.name, curr.line_start, curr.pos_start, curr.line_end,
              curr.pos_end, old.line_start, old.pos_start, old.line_end,
              old.pos_end);
      exit(1);
    }

    if (e->a.v.f.global && d->sc == SC_STATIC) {
      ast_pos old = e->ref->pos;
      ast_pos curr = d->pos;
      fprintf(stderr,
              "global function %s follows non-static "
              "(%d:%d-%d:%d), other decl at %d:%d-%d:%d\n",
              d->v.func.name, curr.line_start, curr.pos_start, curr.line_end,
              curr.pos_end, old.line_start, old.pos_start, old.line_end,
              old.pos_end);
      exit(1);
    }

    global = e->a.v.f.global;
  }

  attrs a;
  a.t = ATTR_FUNC;
  a.v.f.defined = alr_defined || has_body;
  a.v.f.global = global;
  assert(t->t == TYPE_FN);
  add_to_symtable(c, t, d->v.func.name, d, a, d->v.func.name);

  attrs local_a;
  local_a.t = ATTR_LOCAL;

  if (has_body) {
    for (int i = 0; i < d->v.func.params_len; ++i) {
      add_to_symtable(c, d->tp->v.fntype.params[i], d->v.func.params_names[i],
                      NULL, local_a, d->v.func.original_params[i]);
    }

    c->curr_func = d;
    typecheck_block(c, d->v.func.bs->v.block.items,
                    d->v.func.bs->v.block.items_len);

    c->curr_func = NULL;
  }
}

static void typecheck_filescope_var_decl(checker *c, decl *d) {
  struct _init_value iv;

  if (d->v.var.init != NULL) {
    check_for_constant_expr(d->v.var.init);
    iv.t = INIT_INITIAL;

    // FIXME: eval here, (or at some other stage but eval)
    {
      assert(d->v.var.init->t == EXPR_INT_CONST);
      d->v.var.init->v.intc = convert_const(d->v.var.init->v.intc, d->tp);
      iv.v = const_to_initial(d->v.var.init->v.intc);
    }
  } else if (d->sc == SC_EXTERN)
    iv.t = INIT_NOINIT;
  else
    iv.t = INIT_TENTATIVE;

  bool global = d->sc != SC_STATIC;

  syme *old = ht_get(c->st, d->v.var.name);
  if (old != NULL) {
    if (old->t->t == TYPE_FN) {
      ast_pos new_pos = d->pos;
      ast_pos old_pos = old->ref->pos;
      fprintf(stderr,
              "function %s redeclared as var (%d:%d-%d:%d), old decl at "
              "%d:%d-%d:%d\n",
              d->v.var.original_name, new_pos.line_start, new_pos.pos_start,
              new_pos.line_end, new_pos.pos_end, old_pos.line_start,
              old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

      exit(1);
    }

    if (!types_eq(old->t, d->tp)) {
      ast_pos new_pos = d->pos;
      ast_pos old_pos = old->ref->pos;
      fprintf(
          stderr,
          "redeclaration of %s with different type (%d:%d-%d:%d), old decl at "
          "%d:%d-%d:%d\n",
          d->v.var.name, new_pos.line_start, new_pos.pos_start,
          new_pos.line_end, new_pos.pos_end, old_pos.line_start,
          old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

      exit(1);
    }

    assert(old->a.t == ATTR_STATIC);

    if (d->sc == SC_EXTERN) {
      global = old->a.v.s.global;
    } else if (old->a.v.s.global != global) {
      ast_pos new_pos = d->pos;
      ast_pos old_pos = old->ref->pos;
      fprintf(
          stderr,
          "conflicting variable linkage for var %s (%d:%d-%d:%d), old decl at "
          "%d:%d-%d:%d\n",
          d->v.var.original_name, new_pos.line_start, new_pos.pos_start,
          new_pos.line_end, new_pos.pos_end, old_pos.line_start,
          old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

      exit(1);
    }

    if (old->a.v.s.init.t == INIT_INITIAL) {
      if (iv.t == INIT_INITIAL) {
        ast_pos old_pos = old->ref->pos;
        ast_pos new_pos = d->pos;
        fprintf(stderr,
                "conflicting file scope declarations for var %s (%d:%d-%d:%d), "
                "old decl at "
                "%d:%d-%d:%d\n",
                d->v.var.original_name, new_pos.line_start, new_pos.pos_start,
                new_pos.line_end, new_pos.pos_end, old_pos.line_start,
                old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

        exit(1);
      }

      else {
        iv = old->a.v.s.init;
      }
    } else if (iv.t != INIT_INITIAL && old->a.v.s.init.t == INIT_TENTATIVE) {
      iv.t = INIT_TENTATIVE;
    }
  }

  attrs a;
  a.t = ATTR_STATIC;
  a.v.s.global = global;
  a.v.s.init = iv;

  add_to_symtable(c, d->tp, d->v.var.name, d, a, d->v.var.original_name);
}

static void typecheck_local_var_decl(checker *c, decl *d) {
  attrs a;
  switch (d->sc) {
  case SC_EXTERN: {
    if (d->v.var.init != NULL) {
      ast_pos pos = d->pos;
      fprintf(
          stderr,
          "initializer on local extern variable declaration %s (%d:%d-%d:%d)\n",
          d->v.var.original_name, pos.line_start, pos.pos_start, pos.line_end,
          pos.pos_end);

      exit(1);
    }
    syme *old = ht_get(c->st, d->v.var.name);
    if (old != NULL) {
      if (old->t->t == TYPE_FN) {

        ast_pos old_pos = old->ref->pos;
        ast_pos new_pos = d->pos;
        fprintf(stderr,
                "function %s redeclared as var (%d:%d-%d:%d), old decl at "
                "%d:%d-%d:%d\n",
                d->v.var.original_name, old_pos.line_start, old_pos.pos_start,
                old_pos.line_end, old_pos.pos_end, new_pos.line_start,
                new_pos.pos_start, new_pos.line_end, new_pos.pos_end);

        exit(1);
      }

      if (!types_eq(old->t, d->tp)) {
        ast_pos new_pos = d->pos;
        ast_pos old_pos = old->ref->pos;
        fprintf(stderr,
                "redeclaration of %s with different type (%d:%d-%d:%d), old "
                "decl at "
                "%d:%d-%d:%d\n",
                d->v.var.original_name, new_pos.line_start, new_pos.pos_start,
                new_pos.line_end, new_pos.pos_end, old_pos.line_start,
                old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

        exit(1);
      }

      return;
    } else {
      a.t = ATTR_STATIC;
      a.v.s.global = true;
      a.v.s.init.t = INIT_NOINIT;

      add_to_symtable(c, d->tp, d->v.var.name, d, a, d->v.var.original_name);
      return;
    }
    break;

  } break;
  case SC_STATIC: {
    struct _init_value iv;
    iv.t = INIT_INITIAL;
    if (d->v.var.init != NULL) {
      check_for_constant_expr(d->v.var.init);
      d->v.var.init->v.intc = convert_const(d->v.var.init->v.intc, d->tp);
      // FIXME: eval here, (or at some other stage but eval)
      {
        assert(d->v.var.init->t == EXPR_INT_CONST);
        iv.v = const_to_initial(d->v.var.init->v.intc);
      }
    } else {
      int_const tmp;
      tmp.t = CONST_INT;
      tmp.v = 0;
      iv.v = const_to_initial(convert_const(tmp, d->tp));
    }

    a.t = ATTR_STATIC;
    a.v.s.global = false;
    a.v.s.init = iv;
    break;

  } break;
  case SC_NONE: {
    a.t = ATTR_LOCAL;
    add_to_symtable(
        c, d->tp, d->v.var.name, d, a,
        d->v.var.original_name); // important to do before typechecking expr

    if (d->v.var.init != NULL) {
      typecheck_expr(c, d->v.var.init);
      d->v.var.init = convert_to(c, d->v.var.init, d->tp);
    }

    return;

  } break;
  }

  add_to_symtable(c, d->tp, d->v.var.name, d, a, d->v.var.original_name);
}

static void typecheck_var_decl(checker *c, decl *d) {
  if (d->scope == 0)
    typecheck_filescope_var_decl(c, d);
  else
    typecheck_local_var_decl(c, d);
}

static void typecheck_decl(checker *c, decl *d) {
  switch (d->t) {
  case DECL_FUNC:
    typecheck_func_decl(c, d);
    break;
  case DECL_VAR:
    typecheck_var_decl(c, d);
    break;
  default:
    UNREACHABLE();
  }
}

sym_table typecheck(program *p) {
  sym_table st;
  checker c;
  init_checker(&c, p->expr_arena);
  decl *d = p->first_decl;
  for (; d != NULL; d = d->next)
    typecheck_decl(&c, d);

  st.t = c.st;
  st.entry_arena = c.syme_arena;
  return st;
}

static void print_attr(attrs *a) {
  switch (a->t) {
  case ATTR_FUNC:
    printf("func(global: %s, defined: %s)", a->v.f.global ? "true" : "false",
           a->v.f.defined ? "true" : "false");
    break;
  case ATTR_STATIC:
    printf("static(global: %s, ", a->v.s.global ? "true" : "false");
    switch (a->v.s.init.t) {
    case INIT_TENTATIVE:
      printf("tentative");
      break;
    case INIT_INITIAL:
      printf("initial(");
      switch (a->v.s.init.v.t) {
      case INITIAL_INT:
        printf("int");
        break;
      case INITIAL_LONG:
        printf("long");
        break;
      case INITIAL_UINT:
        printf("uint");
        break;
      case INITIAL_ULONG:
        printf("ulong");
        break;
      }

      printf(") (%llu)", (long long unsigned)a->v.s.init.v.v);
      break;
    case INIT_NOINIT:
      printf("no init");
      break;
    }
    printf(")");
    break;
  case ATTR_LOCAL:
    printf("local");
    break;
  }
}

static void print_syme(const char *name, syme *e) {
#define TYPE_BUF_LEN_FOR_PRINT_SYME 256
  EMIT_TYPE_INTO_BUF(type_buf_for_print_syme, 256, e->t);
  if (e->ref != NULL)
    printf("%s(%s) : %s, (%d:%d), ", name, e->original_name,
           type_buf_for_print_syme, e->ref->pos.line_start,
           e->ref->pos.pos_start);
  else
    printf("%s(%s): %s, ", name, e->original_name, type_buf_for_print_syme);
  print_attr(&e->a);
  printf("\n");
}

void print_sym_table(sym_table *st) {
  hti it = ht_iterator(st->t);

  printf("-- SYM TABLE --\n");

  while (ht_next(&it)) {
    syme *e = (syme *)it.value;
    print_syme(it.key, e);
  }
}

void free_sym_table(sym_table *st) { destroy_arena(st->entry_arena); }

static void buf_write(char *buf, size_t size, size_t *pos, const char *s) {
  size_t len = strlen(s);
  if (*pos < size - 1) {
    size_t copy_len = (len < size - 1 - *pos) ? len : (size - 1 - *pos);
    memcpy(buf + *pos, s, copy_len);
    *pos += copy_len;
    buf[*pos] = '\0';
  }
}

void emit_type_name_buf(char *buf, size_t size, size_t *pos, type *t) {
  switch (t->t) {
  case TYPE_INT:
    buf_write(buf, size, pos, "int");
    return;
  case TYPE_LONG:
    buf_write(buf, size, pos, "long");
    return;
  case TYPE_UINT:
    buf_write(buf, size, pos, "uint");
    return;
  case TYPE_ULONG:
    buf_write(buf, size, pos, "ulong");
    return;

  case TYPE_FN:
    emit_type_name_buf(buf, size, pos, t->v.fntype.return_type);
    buf_write(buf, size, pos, "(");
    if (t->v.fntype.param_count >= 1) {
      emit_type_name_buf(buf, size, pos, t->v.fntype.params[0]);
      for (int i = 1; i < t->v.fntype.param_count; ++i) {
        buf_write(buf, size, pos, ", ");
        emit_type_name_buf(buf, size, pos, t->v.fntype.params[i]);
      }
    }
    buf_write(buf, size, pos, ")");
    return;
  }
}

int type_rank(type *t) {
  switch (t->t) {
  case TYPE_INT:
  case TYPE_UINT:
    return 4;
  case TYPE_LONG:
  case TYPE_ULONG:
    return 5;
  case TYPE_FN:
    return -1;
  default:
    UNREACHABLE();
  }
}

bool type_signed(type *t) {
  switch (t->t) {
  case TYPE_INT:
  case TYPE_LONG:
    return true;
  case TYPE_UINT:
  case TYPE_ULONG:
    return false;
  case TYPE_FN:
    return -1;
  default:
    UNREACHABLE();
  }
}
