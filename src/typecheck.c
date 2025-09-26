#include "typecheck.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "parser.h"
#include "table.h"
#include <assert.h>
#include <stdio.h>

typedef struct _checker checker;

static void print_syme(int idx, syme *e);

struct _checker {
  arena *syme_arena;
  arena *type_arena;
  sym_table st;
};

static type *new_type(checker *c, int t) {
  type *res = ARENA_ALLOC_OBJ(c->type_arena, type);
  res->t = t;
  return res;
}

static syme *new_syme(checker *c, type *t, string name, int name_idx,
                      decl *origin, attrs a) {
  syme *e = ARENA_ALLOC_OBJ(c->syme_arena, syme);
  e->name_idx = name_idx;
  e->original_name = name;
  e->ref = origin;
  e->t = t;
  e->a = a;
  return e;
}

static syme *add_to_symtable(checker *c, type *t, string name, int name_idx,
                             decl *origin, attrs a) {
  syme *e;
  ht_set_int(c->st, name_idx, e = new_syme(c, t, name, name_idx, origin, a));

  return e;
}

static bool types_eq(type *t1, type *t2) {
  return t1->t == t2->t && (t1->t == TYPE_FN ? t1->v.fntype.param_count ==
                                                   t2->v.fntype.param_count
                                             : 1);
}

static void typecheck_var_expr(checker *c, expr *e) {
  syme *entry = ht_get_int(c->st, e->v.var.name_idx);
  if (entry->t->t == TYPE_FN) {
    fprintf(stderr, "function name used as variable %s, (%d:%d-%d:%d)\n",
            e->v.var.name, e->pos.line_start, e->pos.pos_start, e->pos.line_end,
            e->pos.pos_end);

    after_error();
  }
}

static void typecheck_expr(checker *c, expr *e);

static void typecheck_fn_call_expr(checker *c, expr *e) {
  syme *entry = ht_get_int(c->st, e->v.var.name_idx);
  if (entry->t->t != TYPE_FN) {
    fprintf(stderr, "var name used as variable %s, (%d:%d-%d:%d)\n",
            e->v.var.name, e->pos.line_start, e->pos.pos_start, e->pos.line_end,
            e->pos.pos_end);

    after_error();
  }

  if (entry->t->v.fntype.param_count != e->v.func_call.args_len) {
    fprintf(stderr,
            "invalid arg count when calling %s, expected %d, got %zu, "
            "(%d:%d-%d:%d)\n",
            e->v.var.name, entry->t->v.fntype.param_count,
            e->v.func_call.args_len, e->pos.line_start, e->pos.pos_start,
            e->pos.line_end, e->pos.pos_end);

    after_error();
  }

  for (int i = 0; i < e->v.func_call.args_len; ++i) {
    typecheck_expr(c, e->v.func_call.args[i]);
  }
}

static void typecheck_expr(checker *c, expr *e) {
  switch (e->t) {
  case EXPR_INT_CONST:
    break;
  case EXPR_UNARY:
    typecheck_expr(c, e->v.u.e);
    break;
  case EXPR_BINARY:
    typecheck_expr(c, e->v.b.l);
    typecheck_expr(c, e->v.b.r);
    break;
  case EXPR_ASSIGNMENT:
    typecheck_expr(c, e->v.assignment.l);
    typecheck_expr(c, e->v.assignment.r);
    break;
  case EXPR_TERNARY:
    typecheck_expr(c, e->v.ternary.cond);
    typecheck_expr(c, e->v.ternary.then);
    typecheck_expr(c, e->v.ternary.elze);
    break;
  case EXPR_VAR:
    typecheck_var_expr(c, e);
    break;
  case EXPR_FUNC_CALL:
    typecheck_fn_call_expr(c, e);
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

static void typecheck_stmt(checker *c, stmt *s) {
  switch (s->t) {
  case STMT_RETURN:
    typecheck_expr(c, s->v.ret.e);
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
        after_error();
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
  case STMT_DEFAULT:
    typecheck_stmt(c, s->v.default_stmt.s);
  case STMT_BREAK:
  case STMT_NULL:
  case STMT_GOTO:
  case STMT_CONTINUE:
    break;
  }
}

static void init_checker(checker *c) {
  NEW_ARENA(c->syme_arena, syme);
  NEW_ARENA(c->type_arena, type);
  vec_push_back(arenas_to_destroy, c->syme_arena);
  vec_push_back(arenas_to_destroy, c->type_arena);

  c->st = ht_create_int();
  vec_push_back(tables_to_destroy, c->st);
}

static void typecheck_func_decl(checker *c, decl *d) {
  type *t = new_type(c, TYPE_FN);
  t->v.fntype.param_count = d->v.func.params_len;
  char has_body = d->v.func.bs != NULL ? 1 : 0;
  char alr_defined = false;
  char global = d->sc != SC_STATIC;

  syme *e = ht_get_int(c->st, d->v.var.name_idx);

  if (d->sc == SC_STATIC && d->scope != 0) {
    ast_pos curr = d->pos;
    fprintf(stderr,
            "function %s is declared in block, but has static storage class"
            "(%d:%d-%d:%d)\n",
            d->v.func.name, curr.line_start, curr.pos_start, curr.line_end,
            curr.pos_end);
    after_error();
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
      after_error();
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
      after_error();
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
      after_error();
    }

    global = e->a.v.f.global;
  }

  attrs a;
  a.t = ATTR_FUNC;
  a.v.f.defined = alr_defined || has_body;
  a.v.f.global = global;
  assert(t->t == TYPE_FN);
  add_to_symtable(c, t, d->v.func.name, d->v.func.name_idx, d, a);

  attrs local_a;
  local_a.t = ATTR_LOCAL;

  if (has_body) {
    for (int i = 0; i < d->v.func.params_len; ++i) {
      add_to_symtable(c, new_type(c, TYPE_INT), d->v.func.params[i],
                      (int)(intptr_t)d->v.func.params_idxs[i], NULL, local_a);
    }

    typecheck_block(c, d->v.func.bs->v.block.items,
                    d->v.func.bs->v.block.items_len);
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
      iv.v = d->v.var.init->v.intc.v;
    }
  } else if (d->sc == SC_EXTERN)
    iv.t = INIT_NOINIT;
  else
    iv.t = INIT_TENTATIVE;

  bool global = d->sc != SC_STATIC;

  syme *old = ht_get_int(c->st, d->v.var.name_idx);
  if (old != NULL) {
    if (old->t->t == TYPE_FN) {
      ast_pos new_pos = d->pos;
      ast_pos old_pos = old->ref->pos;
      fprintf(stderr,
              "function %s redeclared as var (%d:%d-%d:%d), old decl at "
              "%d:%d-%d:%d\n",
              d->v.var.name, new_pos.line_start, new_pos.pos_start,
              new_pos.line_end, new_pos.pos_end, old_pos.line_start,
              old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

      after_error();
    }

    if (old->a.t != ATTR_STATIC) {
      ast_pos new_pos = d->pos;
      ast_pos old_pos = old->ref->pos;
      fprintf(stderr,
              " %s (%d:%d-%d:%d), old decl at "
              "%d:%d-%d:%d\n",
              d->v.var.name, new_pos.line_start, new_pos.pos_start,
              new_pos.line_end, new_pos.pos_end, old_pos.line_start,
              old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

      after_error();
    }

    if (d->sc == SC_EXTERN) {
      global = old->a.v.s.global;
    } else if (old->a.v.s.global != global) {
      ast_pos new_pos = d->pos;
      ast_pos old_pos = old->ref->pos;
      fprintf(
          stderr,
          "conflicting variable linkage for var %s (%d:%d-%d:%d), old decl at "
          "%d:%d-%d:%d\n",
          d->v.var.name, new_pos.line_start, new_pos.pos_start,
          new_pos.line_end, new_pos.pos_end, old_pos.line_start,
          old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

      after_error();
    }

    if (old->a.v.s.init.t == INIT_INITIAL) {
      if (iv.t == INIT_INITIAL) {
        ast_pos old_pos = old->ref->pos;
        ast_pos new_pos = d->pos;
        fprintf(stderr,
                "conflicting file scope declarations for var %s (%d:%d-%d:%d), "
                "old decl at "
                "%d:%d-%d:%d\n",
                d->v.var.name, new_pos.line_start, new_pos.pos_start,
                new_pos.line_end, new_pos.pos_end, old_pos.line_start,
                old_pos.pos_start, old_pos.line_end, old_pos.pos_end);

        after_error();
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

  add_to_symtable(c, new_type(c, TYPE_INT), d->v.var.name, d->v.var.name_idx, d,
                  a);
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
          d->v.var.name, pos.line_start, pos.pos_start, pos.line_end,
          pos.pos_end);

      after_error();
    }
    syme *old = ht_get_int(c->st, d->v.var.name_idx);
    if (old != NULL) {
      if (old->t->t == TYPE_FN) {

        ast_pos old_pos = old->ref->pos;
        ast_pos new_pos = d->pos;
        fprintf(stderr,
                "function %s redeclared as var (%d:%d-%d:%d), old decl at "
                "%d:%d-%d:%d\n",
                d->v.var.name, old_pos.line_start, old_pos.pos_start,
                old_pos.line_end, old_pos.pos_end, new_pos.line_start,
                new_pos.pos_start, new_pos.line_end, new_pos.pos_end);

        after_error();
      }
      return;
    } else {
      a.t = ATTR_STATIC;
      a.v.s.global = true;
      a.v.s.init.t = INIT_NOINIT;

      add_to_symtable(c, new_type(c, TYPE_INT), d->v.var.name,
                      d->v.var.name_idx, d, a);
      return;
    }
    break;

  } break;
  case SC_STATIC: {
    struct _init_value iv;
    iv.t = INIT_INITIAL;
    if (d->v.var.init != NULL) {
      check_for_constant_expr(d->v.var.init);
      // FIXME: eval here, (or at some other stage but eval)
      {
        assert(d->v.var.init->t == EXPR_INT_CONST);
        iv.v = d->v.var.init->v.intc.v;
      }
    } else {
      iv.v = 0;
    }

    a.t = ATTR_STATIC;
    a.v.s.global = false;
    a.v.s.init = iv;
    break;

  } break;
  case SC_NONE: {
    a.t = ATTR_LOCAL;
    add_to_symtable(c, new_type(c, TYPE_INT), d->v.var.name, d->v.var.name_idx,
                    d,
                    a); // important to be begore typechecking expr

    if (d->v.var.init != NULL)
      typecheck_expr(c, d->v.var.init);

    return;

  } break;
  }

  add_to_symtable(c, new_type(c, TYPE_INT), d->v.var.name, d->v.var.name_idx, d,
                  a);
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

ht *typecheck(program *p) {
  checker c;
  init_checker(&c);
  decl *d = p;
  for (; d != NULL; d = d->next)
    typecheck_decl(&c, d);

  return c.st;
}

static const char *type_name(type *t) {
  switch (t->t) {
  case TYPE_INT:
    return "int";
  case TYPE_FN: {
    static char buf[255];
    sprintf(buf, "func(%d)", t->v.fntype.param_count);
    return buf;
  }
  }
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
      printf("initial(%llu)", (unsigned long long)a->v.s.init.v);
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

static void print_syme(int idx, syme *e) {
  if (e->ref != NULL)
    printf("%d : %s (%s) (%d:%d)", idx, e->original_name, type_name(e->t),
           e->ref->pos.line_start, e->ref->pos.pos_start);
  else
    printf("%d : %s (%s)", idx, e->original_name, type_name(e->t));
  print_attr(&e->a);
  printf("\n");
}

void print_sym_table(sym_table st) {
  hti it = ht_iterator(st);

  printf("-- SYM TABLE --\n");

  while (ht_next(&it)) {
    syme *e = (syme *)it.value;
    print_syme(it.idx, e);
  }
}
