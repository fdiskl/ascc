#include "tac.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "parser.h"
#include "vec.h"

void init_tacgen(tacgen *tg) {
  init_arena(&tg->taci_arena);
  init_arena(&tg->tacf_arena);

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

static tacv gen_tac_from_expr(tacgen *tg, expr *e) {
  // for now only int const
  // FIXME: this is tmp
  tacv v;
  v.t = TACV_CONST;
  v.intv = e->v.intc.v;
  return v;
}

static void gen_tac_from_return_stmt(tacgen *tg, return_stmt rs) {
  taci *i = insert_taci(tg, TAC_RET);
  i->src1 = gen_tac_from_expr(tg, rs.e);
}

static void gen_tac_from_stmt(tacgen *tg, stmt *s);

static void gen_tac_from_block_stmt(tacgen *tg, block_stmt bs) {
  for (int i = 0; i < 4; i++) {
    vec_foreach(stmt *, bs.stmts, it) gen_tac_from_stmt(tg, *it);
  }
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
  for (int i = 0; i < 4; i++) {

    vec_foreach(stmt *, fd.body, it) gen_tac_from_stmt(tg, *it);
  }

  res->firsti = tg->head;

  return res;
}

static tacf *gen_tac_from_decl(tacgen *tg, decl *d) {
  switch (d->t) {
  case DECL_FUNC:
    return gen_tac_from_func_decl(tg, d->v.func);
    break;
  case DECL_VAR:
    todo(); // they can't be present in AST for now, so skip
    break;
  }
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
