
#include "x86.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "tac.h"
#include <stdint.h>

void init_x86_asm_gen(x86_asm_gen *ag) {
  init_arena(&ag->instr_arena);
  init_arena(&ag->func_arena);

  ADD_TO_CLEANUP_ARRAY(arenas_to_free, &ag->instr_arena);
  ADD_TO_CLEANUP_ARRAY(arenas_to_free, &ag->func_arena);
}

x86_instr *alloc_x86_instr(x86_asm_gen *ag, int op) {
  x86_instr *res = ARENA_ALLOC_OBJ(&ag->func_arena, x86_instr);
  res->next = NULL;
  res->prev = NULL;
  res->op = op;
  return res;
}

static x86_instr *insert_x86_instr(x86_asm_gen *ag, int op) {
  x86_instr *i = alloc_x86_instr(ag, op);
  if (ag->head == NULL)
    ag->head = i;
  else {
    i->prev = ag->tail;
    ag->tail->next = i;
  }
  ag->tail = i;

  return i;
}

static x86_func *alloc_x86_func(x86_asm_gen *ag, string name) {
  x86_func *res = ARENA_ALLOC_OBJ(&ag->func_arena, x86_func);
  res->next = NULL;
  res->name = name;
  res->first = NULL;
  return res;
}

static x86_op new_x86_imm(uint64_t v) {
  x86_op op;
  op.t = X86_OP_IMM;
  op.v.imm = v;
  return op;
}

static x86_op new_x86_reg(x86_reg reg, int size) {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg.t = reg;
  op.v.reg.size = size;
  return op;
}

static x86_op new_x86_pseudo(int idx) {
  x86_op op;
  op.t = X86_OP_PSEUDO;
  op.v.pseudo_idx = idx;
  return op;
}

static x86_op operand_from_tac_val(tacv v) {
  switch (v.t) {
  case TACV_CONST:
    return new_x86_imm(v.intv);
  case TACV_VAR:
    return new_x86_pseudo(v.var_idx);
  default:
    unreachable();
  }
}

static void gen_asm_from_ret_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *mov = insert_x86_instr(ag, X86_MOV);
  insert_x86_instr(ag, X86_RET);

  mov->v.binary.dst = new_x86_reg(X86_AX, 4);
  mov->v.binary.src = operand_from_tac_val(i->src1);
}

static void gen_asm_from_unary_instr(x86_asm_gen *ag, taci *i) {
  int op;
  switch (i->op) {
  case TAC_NEGATE:
    op = X86_NEG;
    break;
  case TAC_COMPLEMENT:
    op = X86_NOT;
    break;
  default:
    unreachable();
  }

  x86_instr *mov = insert_x86_instr(ag, X86_MOV);

  mov->v.binary.dst = operand_from_tac_val(i->dst);
  mov->v.binary.src = operand_from_tac_val(i->src1);

  x86_instr *u = insert_x86_instr(ag, op);
  u->v.unary.src = operand_from_tac_val(i->dst);
}

static void gen_asm_from_instr(x86_asm_gen *ag, taci *i) {
  switch (i->op) {
  case TAC_RET:
    gen_asm_from_ret_instr(ag, i);
    break;
  case TAC_NEGATE:
  case TAC_COMPLEMENT:
    gen_asm_from_unary_instr(ag, i);
    break;
  default:
    unreachable();
  }
}

static x86_func *gen_asm_from_func(x86_asm_gen *ag, tacf *f) {
  x86_func *func = alloc_x86_func(ag, f->name);
  ag->head = NULL;
  ag->tail = NULL;
  for (taci *i = f->firsti; i != NULL; i = i->next)
    gen_asm_from_instr(ag, i);
  func->first = ag->head;
  return func;
}

x86_func *gen_asm(x86_asm_gen *ag, tacf *tac_first_f) {
  x86_func *head = NULL;
  x86_func *tail = NULL;
  for (tacf *f = tac_first_f; f != NULL; f = f->next) {
    x86_func *res = gen_asm_from_func(ag, f);

// 2 step fix
#ifndef ASM_DONT_FIX_PSEUDO
    x86_instr *i = alloc_x86_instr(ag, X86_ALLOC_STACK);
    i->v.bytes_to_alloc = fix_pseudo_for_func(res);

    i->next = res->first;
    res->first = i;
#endif

#ifndef ASM_DONT_FIX_INSTRUCTIONS
    fix_instructions_for_func(ag, res);
#endif

    res->next = NULL;
    if (head == NULL)
      head = res;
    else
      tail->next = res;
    tail = res;
  }

  return head;
}
