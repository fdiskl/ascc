#include "tac.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "parser.h"
#include "vec.h"
#include <stdint.h>

void init_tacgen(tacgen *tg) {
  INIT_ARENA(&tg->taci_arena, taci);
  INIT_ARENA(&tg->tacf_arena, tacf);

  ADD_TO_CLEANUP_ARRAY(arenas_to_free, &tg->taci_arena);
  ADD_TO_CLEANUP_ARRAY(arenas_to_free, &tg->tacf_arena);
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

static tacv new_tmp() {
  static int idx = 0;
  tacv v;
  v.t = TACV_VAR;
  v.v.var_idx = idx++;
  return v;
}

static int new_label() {
  static int label_idx = 0;
  return label_idx++;
}

static tacv gen_tac_from_int_const_expr(tacgen *_, int_const ic) {
  tacv v;
  v.t = TACV_CONST;
  v.v.intv = ic.v;
  return v;
}

static tacv gen_tac_from_expr(tacgen *tg, expr *e);

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
  }

  taci *i = insert_taci(tg, op);

  i->dst = new_tmp();
  i->src1 = inner_v;

  return i->dst;
}

static tacv gen_tac_from_OR_binary(tacgen *tg, binary b) {
  tacv res = new_tmp();

  tacv v1 = gen_tac_from_expr(tg, b.l);
  taci *jnz1 = insert_taci(tg, TAC_JNZ);
  jnz1->src1 = v1;
  int true_label = jnz1->label_idx = new_label();
  tacv v2 = gen_tac_from_expr(tg, b.r);
  taci *jnz2 = insert_taci(tg, TAC_JNZ);
  jnz2->src1 = v2;
  jnz2->label_idx = true_label;
  taci *cpy1 = insert_taci(tg, TAC_CPY);
  cpy1->dst = res;
  cpy1->src1 = new_const(0);
  taci *jmp = insert_taci(tg, TAC_JMP);
  int end_label = jmp->label_idx = new_label();
  taci *true_label_i = insert_taci(tg, TAC_LABEL);
  true_label_i->label_idx = true_label;
  taci *cpy2 = insert_taci(tg, TAC_CPY);
  cpy2->dst = res;
  cpy2->src1 = new_const(1);
  taci *end_label_i = insert_taci(tg, TAC_LABEL);
  end_label_i->label_idx = end_label;

  return res;
}

static tacv gen_tac_from_AND_binary(tacgen *tg, binary b) {
  tacv res = new_tmp();

  tacv v1 = gen_tac_from_expr(tg, b.l);
  taci *jz1 = insert_taci(tg, TAC_JZ);
  jz1->src1 = v1;
  int false_label = jz1->label_idx = new_label();
  tacv v2 = gen_tac_from_expr(tg, b.r);
  taci *jz2 = insert_taci(tg, TAC_JZ);
  jz2->src1 = v2;
  jz2->label_idx = false_label;
  taci *cpy1 = insert_taci(tg, TAC_CPY);
  cpy1->dst = res;
  cpy1->src1 = new_const(1);
  taci *jmp = insert_taci(tg, TAC_JMP);
  int end_label = jmp->label_idx = new_label();
  taci *false_label_i = insert_taci(tg, TAC_LABEL);
  false_label_i->label_idx = false_label;
  taci *cpy2 = insert_taci(tg, TAC_CPY);
  cpy2->dst = res;
  cpy2->src1 = new_const(0);
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
  i->src1 = v1;
  i->src2 = v2;
  i->dst = new_tmp();

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
  }
  UNREACHABLE();
}

static void gen_tac_from_return_stmt(tacgen *tg, return_stmt rs) {
  tacv e = gen_tac_from_expr(tg, rs.e);
  taci *i = insert_taci(tg, TAC_RET);
  i->src1 = e;
}

static void gen_tac_from_stmt(tacgen *tg, stmt *s);

static void gen_tac_from_block_stmt(tacgen *tg, block_stmt bs) {
  TODO();
  // vec_foreach(stmt *, bs.stmts, it) gen_tac_from_stmt(tg, *it);
}

static void gen_tac_from_stmt(tacgen *tg, stmt *s) {
  switch (s->t) {
  case STMT_RETURN:
    gen_tac_from_return_stmt(tg, s->v.ret);
    break;
  case STMT_BLOCK:
    gen_tac_from_block_stmt(tg, s->v.block);
    break;
  }
}

static tacf *gen_tac_from_func_decl(tacgen *tg, func_decl fd) {
  tacf *res = alloc_tacf(tg, fd.name);

  tg->head = tg->tail = NULL;

  TODO();
  // vec_foreach(stmt *, fd.body, it) gen_tac_from_stmt(tg, *it);

  res->firsti = tg->head;

  return res;
}

static tacf *gen_tac_from_decl(tacgen *tg, decl *d) {
  switch (d->t) {
  case DECL_FUNC:
    return gen_tac_from_func_decl(tg, d->v.func);
    break;
  case DECL_VAR:
    TODO(); // they can't be present in AST for now, so skip
    break;
  }

  UNREACHABLE();
}

tacf *gen_tac(tacgen *tg, program *p) {
  tacf *head = NULL;
  tacf *tail = NULL;
  decl *d;
  for (d = p; d != NULL; d = d->next) {
    tacf *f = gen_tac_from_decl(tg, d);
    if (head == NULL)
      head = f;
    else
      tail->next = f;
    tail = f;
  }

  return head;
}
