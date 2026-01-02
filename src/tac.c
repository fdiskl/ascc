#include "tac.h"
#include "arena.h"
#include "assert.h"
#include "common.h"
#include "parser.h"
#include "strings.h"
#include "table.h"
#include "type.h"
#include "typecheck.h"
#include "vec.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int tmp_var_counter = 0;

static ht *var_map; // used to store if var name alr used

static void init_tacgen(tacgen *tg, sym_table *st) {
  var_map = ht_create();

  NEW_ARENA(tg->taci_arena, taci);
  NEW_ARENA(tg->tac_top_level_arena, tac_top_level);
  NEW_ARENA(tg->tacv_arena, tacv);

  tg->st = st;
}

static int_const new_int_const(int x) {
  int_const res;
  res.t = CONST_INT;
  res.v = x;
  return res;
}

static initial_init new_int_initial_init(int x, type *t) {
  initial_init res;
  assert(t->t != TYPE_FN);
  res.t = t->t == TYPE_INT ? INITIAL_INT : INITIAL_LONG;
  res.v = x;
  return res;
}

static void free_tacgen(tacgen *tg) { ht_destroy(var_map); }

static tac_top_level *alloc_static_var(tacgen *tg, string name) {
  tac_top_level *res = ARENA_ALLOC_OBJ(tg->tac_top_level_arena, tac_top_level);
  res->next = NULL;
  res->is_func = false;
  res->v.v.name = name;
  return res;
}

static tac_top_level *alloc_tacf(tacgen *tg, string name) {
  tac_top_level *res = ARENA_ALLOC_OBJ(tg->tac_top_level_arena, tac_top_level);
  res->next = NULL;
  res->is_func = true;
  res->v.f.name = name;
  return res;
}

static taci *alloc_taci(tacgen *tg, int op) {
  taci *res = ARENA_ALLOC_OBJ(tg->taci_arena, taci);
  res->op = op;
  res->next = NULL;
  return res;
}

static taci *insert_taci(tacgen *tg, int op) {
  taci *i = alloc_taci(tg, op);
  if (tg->head == NULL)
    tg->head = i;
  else
    tg->tail->next = i;
  tg->tail = i;

  return i;
}

static tacv new_const(int_const c) {
  tacv v;
  v.t = TACV_CONST;
  v.v.iconst = c;
  return v;
}

extern int var_name_idx_counter; // defined in resolve.c

static tacv new_tmp(tacgen *tg, type *t) {
  static char buf[256];
  int e;
  // not rly elegant, FIXME
  do {
    sprintf(buf, "t_%d", ++tmp_var_counter);
    e = (int)(intptr_t)ht_get(var_map, buf);
  } while (e);

  tacv v;
  v.t = TACV_VAR;
  string name = new_string(buf);
  v.v.var = name;

  syme *entry = ARENA_ALLOC_OBJ(tg->st->entry_arena, syme);
  entry->original_name = entry->name = name;
  entry->ref = NULL;
  entry->t = t;
  attrs a;
  a.t = ATTR_LOCAL;
  entry->a = a;
  ht_set(tg->st->t, name, entry);

  return v;
}

static tacv new_var(string name) {
  tacv v;
  v.t = TACV_VAR;
  v.v.var = string_sprintf("%s", name);
  return v;
}

extern int label_idx_counter; // defined in resolve.c
static int new_label() { return ++label_idx_counter; }

static tacv gen_tac_from_int_const_expr(tacgen *_, int_const ic) {
  return new_const(ic);
}

static tacv gen_tac_from_expr(tacgen *tg, expr *e);

static tacv gen_inc_dec_prefix(tacgen *tg, expr *e, tacv inner_v) {
  taci *inc_dec =
      insert_taci(tg, e->v.u.t == UNARY_PREFIX_INC ? TAC_INC : TAC_DEC);
  inc_dec->v.s.src1 = inner_v;
  return inner_v;
}

static tacv gen_inc_dec_postfix(tacgen *tg, expr *e, tacv inner_v) {
  taci *cpy = insert_taci(tg, TAC_CPY);
  cpy->dst = new_tmp(tg, e->tp);
  cpy->v.s.src1 = inner_v;
  taci *inc_dec =
      insert_taci(tg, e->v.u.t == UNARY_POSTFIX_INC ? TAC_INC : TAC_DEC);
  inc_dec->v.s.src1 = inner_v;
  return cpy->dst;
}

static tacv gen_tac_from_unary_expr(tacgen *tg, expr *e) {
  tacv inner_v = gen_tac_from_expr(tg, e->v.u.e);

  int op;
  switch (e->v.u.t) {
  case UNARY_NEGATE:
    op = TAC_NEGATE;
    break;
  case UNARY_COMPLEMENT:
    op = TAC_COMPLEMENT;
    break;
  case UNARY_NOT:
    op = TAC_NOT;
    break;
  case UNARY_PREFIX_INC:
  case UNARY_PREFIX_DEC:
    return gen_inc_dec_prefix(tg, e, inner_v);
  case UNARY_POSTFIX_INC:
  case UNARY_POSTFIX_DEC:
    return gen_inc_dec_postfix(tg, e, inner_v);
  }

  taci *i = insert_taci(tg, op);

  i->dst = new_tmp(tg, e->tp);
  i->v.s.src1 = inner_v;

  return i->dst;
}

static tacv gen_tac_from_OR_binary(tacgen *tg, expr *e) {
  tacv res = new_tmp(tg, e->tp);

  tacv v1 = gen_tac_from_expr(tg, e->v.b.l);
  taci *jnz1 = insert_taci(tg, TAC_JNZ);
  jnz1->v.s.src1 = v1;
  int true_label = jnz1->label_idx = new_label();
  tacv v2 = gen_tac_from_expr(tg, e->v.b.r);
  taci *jnz2 = insert_taci(tg, TAC_JNZ);
  jnz2->v.s.src1 = v2;
  jnz2->label_idx = true_label;
  taci *cpy1 = insert_taci(tg, TAC_CPY);
  cpy1->dst = res;
  cpy1->v.s.src1 = new_const(new_int_const(0));
  taci *jmp = insert_taci(tg, TAC_JMP);
  int end_label = jmp->label_idx = new_label();
  taci *true_label_i = insert_taci(tg, TAC_LABEL);
  true_label_i->label_idx = true_label;
  taci *cpy2 = insert_taci(tg, TAC_CPY);
  cpy2->dst = res;
  cpy2->v.s.src1 = new_const(new_int_const(1));
  taci *end_label_i = insert_taci(tg, TAC_LABEL);
  end_label_i->label_idx = end_label;

  return res;
}

static tacv gen_tac_from_AND_binary(tacgen *tg, expr *e) {
  tacv res = new_tmp(tg, e->tp);

  tacv v1 = gen_tac_from_expr(tg, e->v.b.l);
  taci *jz1 = insert_taci(tg, TAC_JZ);
  jz1->v.s.src1 = v1;
  int false_label = jz1->label_idx = new_label();
  tacv v2 = gen_tac_from_expr(tg, e->v.b.r);
  taci *jz2 = insert_taci(tg, TAC_JZ);
  jz2->v.s.src1 = v2;
  jz2->label_idx = false_label;
  taci *cpy1 = insert_taci(tg, TAC_CPY);
  cpy1->dst = res;
  cpy1->v.s.src1 = new_const(new_int_const(1));
  taci *jmp = insert_taci(tg, TAC_JMP);
  int end_label = jmp->label_idx = new_label();
  taci *false_label_i = insert_taci(tg, TAC_LABEL);
  false_label_i->label_idx = false_label;
  taci *cpy2 = insert_taci(tg, TAC_CPY);
  cpy2->dst = res;
  cpy2->v.s.src1 = new_const(new_int_const(0));
  taci *end_label_i = insert_taci(tg, TAC_LABEL);
  end_label_i->label_idx = end_label;

  return res;
}

static tacv gen_tac_from_binary_expr(tacgen *tg, expr *e) {
#define b(bin, tac)                                                            \
  case bin:                                                                    \
    op = tac;                                                                  \
    break;

  int op;

  switch (e->v.b.t) {
    b(BINARY_ADD, TAC_ADD);
    b(BINARY_SUB, TAC_SUB);
    b(BINARY_MUL, TAC_MUL);
    b(BINARY_DIV, TAC_DIV);
    b(BINARY_MOD, TAC_MOD);
    b(BINARY_BITWISE_AND, TAC_AND);
    b(BINARY_BITWISE_OR, TAC_OR);
    b(BINARY_XOR, TAC_XOR);
    b(BINARY_LSHIFT, TAC_LSHIFT);
    b(BINARY_RSHIFT, TAC_RSHIFT);
    b(BINARY_EQ, TAC_EQ);
    b(BINARY_NE, TAC_NE);
    b(BINARY_LT, TAC_LT);
    b(BINARY_LE, TAC_LE);
    b(BINARY_GT, TAC_GT);
    b(BINARY_GE, TAC_GE);
    break;
  case BINARY_OR:
    return gen_tac_from_OR_binary(tg, e);
  case BINARY_AND:
    return gen_tac_from_AND_binary(tg, e);
  }

  tacv v1 = gen_tac_from_expr(tg, e->v.b.l);
  tacv v2 = gen_tac_from_expr(tg, e->v.b.r);

  taci *i = insert_taci(tg, op);
  i->v.s.src1 = v1;
  i->v.s.src2 = v2;
  i->dst = new_tmp(tg, e->tp);

  return i->dst;
}

static tacv gen_tac_from_assignment_expr(tacgen *tg, assignment a) {
  int op;

  switch (a.t) {
  case ASSIGN:
    op = TAC_CPY;
    break;
  case ASSIGN_ADD:
    op = TAC_ASADD;
    break;
  case ASSIGN_SUB:
    op = TAC_ASSUB;
    break;
  case ASSIGN_MUL:
    op = TAC_ASMUL;
    break;
  case ASSIGN_DIV:
    op = TAC_ASDIV;
    break;
  case ASSIGN_MOD:
    op = TAC_ASMOD;
    break;
  case ASSIGN_AND:
    op = TAC_ASAND;
    break;
  case ASSIGN_OR:
    op = TAC_ASOR;
    break;
  case ASSIGN_XOR:
    op = TAC_ASXOR;
    break;
  case ASSIGN_LSHIFT:
    op = TAC_ASLSHIFT;
    break;
  case ASSIGN_RSHIFT:
    op = TAC_ASRSHIFT;
    break;
  }

  tacv dst = gen_tac_from_expr(tg, a.l);
  tacv src = gen_tac_from_expr(tg, a.r);

  taci *instr = insert_taci(tg, op);
  instr->dst = dst;
  instr->v.s.src1 = src;

  return dst;
}

static tacv gen_tac_from_ternary_expr(tacgen *tg, expr *e) {
  tacv dst = new_tmp(tg, e->tp);
  tacv condv = gen_tac_from_expr(tg, e->v.ternary.cond);

  taci *jz = insert_taci(tg, TAC_JZ);
  int else_label = jz->label_idx = new_label();
  jz->v.s.src1 = condv;

  tacv thenv = gen_tac_from_expr(tg, e->v.ternary.then);
  taci *cpy_then = insert_taci(tg, TAC_CPY);
  cpy_then->dst = dst;
  cpy_then->v.s.src1 = thenv;

  taci *j = insert_taci(tg, TAC_JMP);
  int end_label = j->label_idx = new_label();

  insert_taci(tg, TAC_LABEL)->label_idx = else_label;
  tacv elzev = gen_tac_from_expr(tg, e->v.ternary.elze);
  taci *cpy_else = insert_taci(tg, TAC_CPY);
  cpy_else->dst = dst;
  cpy_else->v.s.src1 = elzev;

  insert_taci(tg, TAC_LABEL)->label_idx = end_label;

  return dst;
}

static tacv gen_tac_from_func_call_expr(tacgen *tg, expr *e) {
  VEC(tacv) v;
  vec_init(v);

  func_call_expr fe = e->v.func_call;
  if (fe.args != NULL)
    for (int i = 0; i < fe.args_len; ++i)
      vec_push_back(v, gen_tac_from_expr(tg, fe.args[i]));

  taci *i = insert_taci(tg, TAC_CALL);
  i->dst = new_tmp(tg, e->tp);
  i->v.call.name = fe.name;

  syme *entry = ht_get(tg->st->t, fe.name);
  assert(entry);
  assert(entry->a.t == ATTR_FUNC);
  if (entry->a.v.f.defined)
    i->v.call.plt = 0;
  else
    i->v.call.plt = 1;

  if (fe.args != NULL) {
    i->v.call.args_len = v.size;
    vec_move_into_arena(tg->tacv_arena, v, tacv, i->v.call.args);
  } else {
    i->v.call.args_len = 0;
    i->v.call.args = NULL;
  }

  return i->dst;
}

static tacv gen_tac_from_cast_expr(tacgen *tg, cast_expr cast) {
  tacv v = gen_tac_from_expr(tg, cast.e);
  type *inner_type = cast.e->tp;
  type *t = cast.tp;
  tacv dst = new_tmp(tg, cast.tp);

  int op;
  if (type_rank(t) == type_rank(inner_type))
    op = TAC_CPY;
  else if (type_rank(t) < type_rank(inner_type))
    op = TAC_TRUNCATE;
  else if (type_signed(inner_type))
    op = TAC_SIGN_EXTEND;
  else
    op = TAC_ZERO_EXTEND;

  taci *i = insert_taci(tg, op);

  i->dst = dst;
  i->v.s.src1 = v;

  return dst;
}

static tacv gen_tac_from_expr(tacgen *tg, expr *e) {
  switch (e->t) {
  case EXPR_INT_CONST:
    return gen_tac_from_int_const_expr(tg, e->v.intc);
  case EXPR_UNARY:
    return gen_tac_from_unary_expr(tg, e);
  case EXPR_BINARY:
    return gen_tac_from_binary_expr(tg, e);
  case EXPR_ASSIGNMENT:
    return gen_tac_from_assignment_expr(tg, e->v.assignment);
  case EXPR_VAR:
    return new_var(e->v.var.name);
  case EXPR_TERNARY:
    return gen_tac_from_ternary_expr(tg, e);
  case EXPR_FUNC_CALL:
    return gen_tac_from_func_call_expr(tg, e);
  case EXPR_CAST:
    return gen_tac_from_cast_expr(tg, e->v.cast);
  }
  UNREACHABLE();
}

static void gen_tac_from_return_stmt(tacgen *tg, return_stmt rs) {
  tacv e = gen_tac_from_expr(tg, rs.e);
  taci *i = insert_taci(tg, TAC_RET);
  i->v.s.src1 = e;
}

static void gen_tac_from_stmt(tacgen *tg, stmt *s);
static void gen_tac_from_decl(tacgen *tg, decl *d);

static void gen_tac_from_block_item(tacgen *tg, block_item bi) {
  if (bi.d != NULL)
    gen_tac_from_decl(tg, bi.d);
  else
    gen_tac_from_stmt(tg, bi.s);
}

static void gen_tac_from_block_stmt(tacgen *tg, block_stmt bs) {
  for (int i = 0; i < bs.items_len; ++i)
    gen_tac_from_block_item(tg, bs.items[i]);
}

static void gen_tac_from_if_stmt(tacgen *tg, if_stmt is) {
  tacv condv = gen_tac_from_expr(tg, is.cond);
  taci *jz = insert_taci(tg, TAC_JZ);
  int else_label = jz->label_idx = new_label();
  jz->v.s.src1 = condv;
  gen_tac_from_stmt(tg, is.then);
  taci *j = insert_taci(tg, TAC_JMP);
  int end_label = j->label_idx = new_label();
  insert_taci(tg, TAC_LABEL)->label_idx = else_label;
  if (is.elze != NULL) {
    gen_tac_from_stmt(tg, is.elze);
  }
  insert_taci(tg, TAC_LABEL)->label_idx = end_label;
}

static void gen_tac_from_goto_stmt(tacgen *tg, goto_stmt gs) {
  insert_taci(tg, TAC_JMP)->label_idx = gs.label_idx;
}

static void gen_tac_from_label_stmt(tacgen *tg, label_stmt ls) {
  insert_taci(tg, TAC_LABEL)->label_idx = ls.label_idx;
  gen_tac_from_stmt(tg, ls.s);
}

static void gen_tac_from_break_stmt(tacgen *tg, break_stmt b) {
  insert_taci(tg, TAC_JMP)->label_idx = b.idx;
}

static void gen_tac_from_continue_stmt(tacgen *tg, continue_stmt c) {
  insert_taci(tg, TAC_JMP)->label_idx = c.idx;
}

static void gen_tac_from_default_stmt(tacgen *tg, default_stmt d) {
  insert_taci(tg, TAC_LABEL)->label_idx = d.label_idx;
  gen_tac_from_stmt(tg, d.s);
}

static void gen_tac_from_case_stmt(tacgen *tg, case_stmt c) {
  insert_taci(tg, TAC_LABEL)->label_idx = c.label_idx;
  gen_tac_from_stmt(tg, c.s);
}

static void gen_tac_from_while_stmt(tacgen *tg, while_stmt w) {
  insert_taci(tg, TAC_LABEL)->label_idx = w.continue_label_idx;

  tacv v = gen_tac_from_expr(tg, w.cond);

  taci *jz = insert_taci(tg, TAC_JZ);
  jz->v.s.src1 = v;
  jz->label_idx = w.break_label_idx;

  gen_tac_from_stmt(tg, w.s);

  insert_taci(tg, TAC_JMP)->label_idx = w.continue_label_idx;

  insert_taci(tg, TAC_LABEL)->label_idx = w.break_label_idx;
}

static void gen_tac_from_dowhile_stmt(tacgen *tg, dowhile_stmt w) {

  int start = insert_taci(tg, TAC_LABEL)->label_idx = new_label();

  gen_tac_from_stmt(tg, w.s);

  insert_taci(tg, TAC_LABEL)->label_idx = w.continue_label_idx;
  tacv v = gen_tac_from_expr(tg, w.cond);

  taci *jnz = insert_taci(tg, TAC_JNZ);
  jnz->v.s.src1 = v;
  jnz->label_idx = start;

  insert_taci(tg, TAC_LABEL)->label_idx = w.break_label_idx;
}

static void gen_tac_from_for_stmt(tacgen *tg, for_stmt f) {
  if (f.init_d != NULL)
    gen_tac_from_decl(tg, f.init_d);
  else if (f.init_e != NULL)
    gen_tac_from_expr(tg, f.init_e);

  int start_label = insert_taci(tg, TAC_LABEL)->label_idx = new_label();

  tacv condv;
  if (f.cond != NULL)
    condv = gen_tac_from_expr(tg, f.cond);
  else
    condv = new_const(new_int_const(1));

  taci *jz = insert_taci(tg, TAC_JZ);
  jz->v.s.src1 = condv;
  jz->label_idx = f.break_label_idx;

  gen_tac_from_stmt(tg, f.s);

  insert_taci(tg, TAC_LABEL)->label_idx = f.continue_label_idx;

  if (f.post != NULL)
    gen_tac_from_expr(tg, f.post);

  insert_taci(tg, TAC_JMP)->label_idx = start_label;

  insert_taci(tg, TAC_LABEL)->label_idx = f.break_label_idx;
}

static void gen_tac_from_switch_stmt(tacgen *tg, switch_stmt s) {
  tacv condv = gen_tac_from_expr(tg, s.e);

  for (int i = 0; i < s.cases_len; ++i) {
    taci *je = insert_taci(tg, TAC_JE);
    je->label_idx = s.cases[i]->v.case_stmt.label_idx;
    assert(s.cases[i]->v.case_stmt.e->t == EXPR_INT_CONST);
    je->v.s.src1 = condv;
    je->v.s.src2 = new_const(s.cases[i]->v.case_stmt.e->v.intc);
  }

  if (s.default_stmt != NULL) {
    insert_taci(tg, TAC_JMP)->label_idx =
        s.default_stmt->v.default_stmt.label_idx;
  } else {
    insert_taci(tg, TAC_JMP)->label_idx = s.break_label_idx;
  }

  gen_tac_from_stmt(tg, s.s);

  insert_taci(tg, TAC_LABEL)->label_idx = s.break_label_idx;
}

static void gen_tac_from_stmt(tacgen *tg, stmt *s) {
  switch (s->t) {
  case STMT_RETURN:
    gen_tac_from_return_stmt(tg, s->v.ret);
    break;
  case STMT_BLOCK:
    gen_tac_from_block_stmt(tg, s->v.block);
    break;
  case STMT_EXPR:
    gen_tac_from_expr(tg, s->v.e);
    break;
  case STMT_NULL:
    break;
  case STMT_IF:
    gen_tac_from_if_stmt(tg, s->v.if_stmt);
    break;
  case STMT_GOTO:
    gen_tac_from_goto_stmt(tg, s->v.goto_stmt);
    break;
  case STMT_LABEL:
    gen_tac_from_label_stmt(tg, s->v.label);
    break;
  case STMT_WHILE:
    gen_tac_from_while_stmt(tg, s->v.while_stmt);
    break;
  case STMT_DOWHILE:
    gen_tac_from_dowhile_stmt(tg, s->v.dowhile_stmt);
    break;
  case STMT_FOR:
    gen_tac_from_for_stmt(tg, s->v.for_stmt);
    break;
  case STMT_BREAK:
    gen_tac_from_break_stmt(tg, s->v.break_stmt);
    break;
  case STMT_CONTINUE:
    gen_tac_from_continue_stmt(tg, s->v.continue_stmt);
    break;
  case STMT_CASE:
    gen_tac_from_case_stmt(tg, s->v.case_stmt);
    break;
  case STMT_DEFAULT:
    gen_tac_from_default_stmt(tg, s->v.default_stmt);
    break;
  case STMT_SWITCH:
    gen_tac_from_switch_stmt(tg, s->v.switch_stmt);
    break;
  }
}

static tac_top_level *gen_tac_from_func_decl(tacgen *tg, func_decl fd) {
  if (fd.bs == NULL)
    return NULL;
  tac_top_level *res = alloc_tacf(tg, fd.name);
  res->v.f.params = fd.params_names;
  res->v.f.params_len = fd.params_len;

  tg->head = tg->tail = NULL;

  for (int i = 0; i < fd.bs->v.block.items_len; ++i)
    gen_tac_from_block_item(tg, fd.bs->v.block.items[i]);

  taci *ret_at_end = insert_taci(tg, TAC_RET);
  ret_at_end->v.s.src1 = new_const(new_int_const(0));

  res->v.f.firsti = tg->head;

  syme *e = ht_get(tg->st->t, res->v.f.name);
  assert(e);
  assert(e->a.t == ATTR_FUNC);

  res->v.f.global = e->a.v.f.global;

  return res;
}

static void gen_tac_from_var_decl(tacgen *tg, decl *d) {
  if (d->sc != SC_NONE || d->scope == 0) // we don't generate tac for file scope
                                         // vars, and local extern/static ones
    return;
  var_decl vd = d->v.var;

  if (vd.init != NULL) {
    tacv dst = new_var(vd.name);
    tacv src = gen_tac_from_expr(tg, vd.init);

    taci *cpy = insert_taci(tg, TAC_CPY);
    cpy->dst = dst;
    cpy->v.s.src1 = src;
  }
}

static void gen_tac_from_decl(tacgen *tg, decl *d) {
  switch (d->t) {
  case DECL_FUNC:
    gen_tac_from_func_decl(tg, d->v.func);
    break;
  case DECL_VAR:
    gen_tac_from_var_decl(tg, d);
    break;
  }
}

tac_program gen_tac(program *p, sym_table *st) {
  tacgen tg;
  tac_program res;
  init_tacgen(&tg, st);

  res.tac_top_level_arena = tg.tac_top_level_arena;
  res.taci_arena = tg.taci_arena;
  res.tacv_arena = tg.tacv_arena;

  // write all var names into map
  for (decl *d = p->first_decl; d != NULL; d = d->next) {
    if (d->t == DECL_VAR)
      ht_set(var_map, d->v.var.name, (void *)(intptr_t)1);
    if (d->t == DECL_FUNC) // funcs too just in case
      ht_set(var_map, d->v.func.name, (void *)(intptr_t)1);
  }

  tac_top_level *head = NULL;
  tac_top_level *tail = NULL;
  for (decl *d = p->first_decl; d != NULL; d = d->next) {
    if (d->t != DECL_FUNC)
      continue;
    tac_top_level *f = gen_tac_from_func_decl(&tg, d->v.func);
    if (f == NULL)
      continue;
    if (head == NULL)
      head = f;
    else
      tail->next = f;
    tail = f;
  }

  hti it = ht_iterator(tg.st->t);
  while (ht_next(&it)) {
    syme *e = it.value;
    if (e->a.t == ATTR_STATIC) {
      switch (e->a.v.s.init.t) {
      case INIT_TENTATIVE: {
        tac_top_level *sv = alloc_static_var(&tg, e->name);
        sv->v.v.global = e->a.v.s.global;
        sv->v.v.init = new_int_initial_init(0, e->t);
        tail->next = sv;
        tail = sv;
        break;
      }
      case INIT_INITIAL: {
        tac_top_level *sv = alloc_static_var(&tg, e->name);
        sv->v.v.global = e->a.v.s.global;
        sv->v.v.init = e->a.v.s.init.v;
        tail->next = sv;
        tail = sv;
        break;
      }
      case INIT_NOINIT:
        continue;
      }
    }
  }

  res.first = head;
  free_tacgen(&tg);

  return res;
}

void free_tac(tac_program *prog) {
  destroy_arena(prog->tac_top_level_arena);
  destroy_arena(prog->taci_arena);
  destroy_arena(prog->tacv_arena);
}
