#include "tac.h"
#include "arena.h"
#include "assert.h"
#include "common.h"
#include "driver.h"
#include "parser.h"
#include "strings.h"
#include "vec.h"
#include <stdint.h>
#include <stdio.h>

static int tmp_var_counter = 0;

void init_tacgen(tacgen *tg) {
  INIT_ARENA(&tg->taci_arena, taci);
  INIT_ARENA(&tg->tacf_arena, tacf);
  INIT_ARENA(&tg->tacv_arena, tacv);

  vec_push_back(arenas_to_free, &tg->taci_arena);
  vec_push_back(arenas_to_free, &tg->tacf_arena);
  vec_push_back(arenas_to_free, &tg->tacv_arena);
}

static tacf *alloc_tacf(tacgen *tg, string name) {
  tacf *res = ARENA_ALLOC_OBJ(&tg->tacf_arena, tacf);
  res->next = NULL;
  res->name = name;
  return res;
}

static taci *alloc_taci(tacgen *tg, int op) {
  taci *res = ARENA_ALLOC_OBJ(&tg->taci_arena, taci);
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

static tacv new_const(uint64_t c) {
  tacv v;
  v.t = TACV_CONST;
  v.v.intv = c;
  return v;
}

extern int var_name_idx_counter; // defined in resolve.c

static tacv new_tmp() {
  tacv v;
  v.t = TACV_VAR;
  v.v.var = string_sprintf("t_%d", ++tmp_var_counter);
  return v;
}

static tacv new_var(string name) {
  tacv v;
  v.t = TACV_VAR;
  v.v.var = string_sprintf("_%s", name);
  return v;
}

extern int label_idx_counter; // defined in resolve.c
static int new_label() { return ++label_idx_counter; }

static tacv gen_tac_from_int_const_expr(tacgen *_, int_const ic) {
  tacv v;
  v.t = TACV_CONST;
  v.v.intv = ic.v;
  return v;
}

static tacv gen_tac_from_expr(tacgen *tg, expr *e);

static tacv gen_inc_dec_prefix(tacgen *tg, unary u, tacv inner_v) {
  taci *inc_dec = insert_taci(tg, u.t == UNARY_PREFIX_INC ? TAC_INC : TAC_DEC);
  inc_dec->v.s.src1 = inner_v;
  return inner_v;
}

static tacv gen_inc_dec_postfix(tacgen *tg, unary u, tacv inner_v) {
  taci *cpy = insert_taci(tg, TAC_CPY);
  cpy->dst = new_tmp();
  cpy->v.s.src1 = inner_v;
  taci *inc_dec = insert_taci(tg, u.t == UNARY_POSTFIX_INC ? TAC_INC : TAC_DEC);
  inc_dec->v.s.src1 = inner_v;
  return cpy->dst;
}

static tacv gen_tac_from_unary_expr(tacgen *tg, unary u) {
  tacv inner_v = gen_tac_from_expr(tg, u.e);

  int op;
  switch (u.t) {
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
    return gen_inc_dec_prefix(tg, u, inner_v);
  case UNARY_POSTFIX_INC:
  case UNARY_POSTFIX_DEC:
    return gen_inc_dec_postfix(tg, u, inner_v);
  }

  taci *i = insert_taci(tg, op);

  i->dst = new_tmp();
  i->v.s.src1 = inner_v;

  return i->dst;
}

static tacv gen_tac_from_OR_binary(tacgen *tg, binary b) {
  tacv res = new_tmp();

  tacv v1 = gen_tac_from_expr(tg, b.l);
  taci *jnz1 = insert_taci(tg, TAC_JNZ);
  jnz1->v.s.src1 = v1;
  int true_label = jnz1->label_idx = new_label();
  tacv v2 = gen_tac_from_expr(tg, b.r);
  taci *jnz2 = insert_taci(tg, TAC_JNZ);
  jnz2->v.s.src1 = v2;
  jnz2->label_idx = true_label;
  taci *cpy1 = insert_taci(tg, TAC_CPY);
  cpy1->dst = res;
  cpy1->v.s.src1 = new_const(0);
  taci *jmp = insert_taci(tg, TAC_JMP);
  int end_label = jmp->label_idx = new_label();
  taci *true_label_i = insert_taci(tg, TAC_LABEL);
  true_label_i->label_idx = true_label;
  taci *cpy2 = insert_taci(tg, TAC_CPY);
  cpy2->dst = res;
  cpy2->v.s.src1 = new_const(1);
  taci *end_label_i = insert_taci(tg, TAC_LABEL);
  end_label_i->label_idx = end_label;

  return res;
}

static tacv gen_tac_from_AND_binary(tacgen *tg, binary b) {
  tacv res = new_tmp();

  tacv v1 = gen_tac_from_expr(tg, b.l);
  taci *jz1 = insert_taci(tg, TAC_JZ);
  jz1->v.s.src1 = v1;
  int false_label = jz1->label_idx = new_label();
  tacv v2 = gen_tac_from_expr(tg, b.r);
  taci *jz2 = insert_taci(tg, TAC_JZ);
  jz2->v.s.src1 = v2;
  jz2->label_idx = false_label;
  taci *cpy1 = insert_taci(tg, TAC_CPY);
  cpy1->dst = res;
  cpy1->v.s.src1 = new_const(1);
  taci *jmp = insert_taci(tg, TAC_JMP);
  int end_label = jmp->label_idx = new_label();
  taci *false_label_i = insert_taci(tg, TAC_LABEL);
  false_label_i->label_idx = false_label;
  taci *cpy2 = insert_taci(tg, TAC_CPY);
  cpy2->dst = res;
  cpy2->v.s.src1 = new_const(0);
  taci *end_label_i = insert_taci(tg, TAC_LABEL);
  end_label_i->label_idx = end_label;

  return res;
}

static tacv gen_tac_from_binary_expr(tacgen *tg, binary b) {
#define b(bin, tac)                                                            \
  case bin:                                                                    \
    op = tac;                                                                  \
    break;

  int op;

  switch (b.t) {
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
    return gen_tac_from_OR_binary(tg, b);
  case BINARY_AND:
    return gen_tac_from_AND_binary(tg, b);
  }

  tacv v1 = gen_tac_from_expr(tg, b.l);
  tacv v2 = gen_tac_from_expr(tg, b.r);

  taci *i = insert_taci(tg, op);
  i->v.s.src1 = v1;
  i->v.s.src2 = v2;
  i->dst = new_tmp();

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

static tacv gen_tac_from_ternary_expr(tacgen *tg, ternary_expr te) {
  tacv dst = new_tmp();
  tacv condv = gen_tac_from_expr(tg, te.cond);

  taci *jz = insert_taci(tg, TAC_JZ);
  int else_label = jz->label_idx = new_label();
  jz->v.s.src1 = condv;

  tacv thenv = gen_tac_from_expr(tg, te.then);
  taci *cpy_then = insert_taci(tg, TAC_CPY);
  cpy_then->dst = dst;
  cpy_then->v.s.src1 = thenv;

  taci *j = insert_taci(tg, TAC_JMP);
  int end_label = j->label_idx = new_label();

  insert_taci(tg, TAC_LABEL)->label_idx = else_label;
  tacv elzev = gen_tac_from_expr(tg, te.elze);
  taci *cpy_else = insert_taci(tg, TAC_CPY);
  cpy_else->dst = dst;
  cpy_else->v.s.src1 = elzev;

  insert_taci(tg, TAC_LABEL)->label_idx = end_label;

  return dst;
}

static tacv gen_tac_from_func_call_expr(tacgen *tg, func_call_expr fe) {
  VEC(tacv) v;
  vec_init(v);

  if (fe.args != NULL)
    for (int i = 0; i < fe.args_len; ++i)
      vec_push_back(v, gen_tac_from_expr(tg, fe.args[i]));

  taci *i = insert_taci(tg, TAC_CALL);
  i->dst = new_tmp();
  i->v.call.name = fe.name;

  if (fe.args != NULL) {
    i->v.call.args_len = v.size;
    vec_move_into_arena(&tg->tacv_arena, v, tacv, i->v.call.args);
  } else {
    i->v.call.args_len = 0;
    i->v.call.args = NULL;
  }

  return i->dst;
}

static tacv gen_tac_from_expr(tacgen *tg, expr *e) {
  switch (e->t) {
  case EXPR_INT_CONST:
    return gen_tac_from_int_const_expr(tg, e->v.intc);
  case EXPR_UNARY:
    return gen_tac_from_unary_expr(tg, e->v.u);
  case EXPR_BINARY:
    return gen_tac_from_binary_expr(tg, e->v.b);
  case EXPR_ASSIGNMENT:
    return gen_tac_from_assignment_expr(tg, e->v.assignment);
    break;
  case EXPR_VAR:
    return new_var(e->v.var.name);
    break;
  case EXPR_TERNARY:
    return gen_tac_from_ternary_expr(tg, e->v.ternary);
    break;
  case EXPR_FUNC_CALL:
    return gen_tac_from_func_call_expr(tg, e->v.func_call);
    break;
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
    condv = new_const(1);

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
    je->v.s.src2 = new_const(s.cases[i]->v.case_stmt.e->v.intc.v);
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

static tacf *gen_tac_from_func_decl(tacgen *tg, func_decl fd) {
  tmp_var_counter = 0;
  if (fd.bs == NULL)
    return NULL;
  tacf *res = alloc_tacf(tg, fd.name);
  res->params = fd.params_names;
  res->params_len = fd.params_len;

  tg->head = tg->tail = NULL;

  for (int i = 0; i < fd.bs->v.block.items_len; ++i)
    gen_tac_from_block_item(tg, fd.bs->v.block.items[i]);

  taci *ret_at_end = insert_taci(tg, TAC_RET);
  ret_at_end->v.s.src1 = new_const(0);

  res->firsti = tg->head;

  return res;
}

static void gen_tac_from_var_decl(tacgen *tg, var_decl vd) {
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
    gen_tac_from_var_decl(tg, d->v.var);
    break;
  }
}

tacf *gen_tac(tacgen *tg, program *p) {
  tacf *head = NULL;
  tacf *tail = NULL;
  decl *d;
  for (d = p; d != NULL; d = d->next) {
    if (d->t != DECL_FUNC)
      continue;
    tacf *f = gen_tac_from_func_decl(tg, d->v.func);
    if (f == NULL)
      continue;
    if (head == NULL)
      head = f;
    else
      tail->next = f;
    tail = f;
  }

  return head;
}
