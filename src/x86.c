
#include "x86.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "tac.h"
#include <stdint.h>
#include <stdio.h>

void init_x86_asm_gen(x86_asm_gen *ag) {
  INIT_ARENA(&ag->instr_arena, x86_instr);
  INIT_ARENA(&ag->func_arena, x86_func);

  vec_push_back(arenas_to_free, &ag->instr_arena);
  vec_push_back(arenas_to_free, &ag->func_arena);
}

x86_instr *alloc_x86_instr(x86_asm_gen *ag, int op) {
  x86_instr *res = ARENA_ALLOC_OBJ(&ag->instr_arena, x86_instr);
  res->next = NULL;
  res->prev = NULL;
  res->op = op;
  res->origin = NULL;
  return res;
}

static x86_instr *insert_x86_instr(x86_asm_gen *ag, int op, taci *origin) {
  x86_instr *i = alloc_x86_instr(ag, op);
  if (ag->head == NULL)
    ag->head = i;
  else {
    i->prev = ag->tail;
    ag->tail->next = i;
  }
  ag->tail = i;

  i->origin = origin;

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

static x86_op new_x86_reg(x86_reg reg) {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg = reg;
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
    return new_x86_imm(v.v.intv);
  case TACV_VAR:
    return new_x86_pseudo(v.v.var_idx);
  }
  UNREACHABLE();
}

static void gen_asm_from_ret_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  insert_x86_instr(ag, X86_RET, i);

  mov->v.binary.dst = new_x86_reg(X86_AX);
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
    UNREACHABLE();
  }

  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);

  mov->v.binary.dst = operand_from_tac_val(i->dst);
  mov->v.binary.src = operand_from_tac_val(i->src1);

  x86_instr *u = insert_x86_instr(ag, op, i);
  u->v.unary.src = operand_from_tac_val(i->dst);
}

static void gen_asm_from_binary_instr(x86_asm_gen *ag, taci *i) {
  if (i->op == TAC_DIV || i->op == TAC_MOD) {
    x86_instr *mov1 = insert_x86_instr(ag, X86_MOV, i);
    insert_x86_instr(ag, X86_CDQ, i);
    x86_instr *idiv = insert_x86_instr(ag, X86_IDIV, i);
    x86_instr *mov2 = insert_x86_instr(ag, X86_MOV, i);

    mov1->v.binary.dst = new_x86_reg(X86_AX);
    mov1->v.binary.src = operand_from_tac_val(i->src1);

    idiv->v.unary.src = operand_from_tac_val(i->src2);

    mov2->v.binary.src = new_x86_reg(i->op == TAC_DIV ? X86_AX : X86_DX);
    mov2->v.binary.dst = operand_from_tac_val(i->dst);

    return;
  }

  int op;
  switch (i->op) {
  case TAC_ADD:
    op = X86_ADD;
    break;
  case TAC_SUB:
    op = X86_SUB;
    break;
  case TAC_MUL:
    op = X86_MULT;
    break;
  case TAC_AND:
    op = X86_AND;
    break;
  case TAC_OR:
    op = X86_OR;
    break;
  case TAC_XOR:
    op = X86_XOR;
    break;
  case TAC_LSHIFT:
    op = X86_SHL;
    break;
  case TAC_RSHIFT:
    op = X86_SAR;
    break;
  default:
    UNREACHABLE();
  }

  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  x86_instr *bini = insert_x86_instr(ag, op, i);

  mov->v.binary.dst = operand_from_tac_val(i->dst);
  mov->v.binary.src = operand_from_tac_val(i->src1);

  bini->v.binary.dst = mov->v.binary.dst;
  bini->v.binary.src = operand_from_tac_val(i->src2);
}

static void gen_asm_from_not_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *cmp = insert_x86_instr(ag, X86_CMP, i);
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  x86_instr *sete = insert_x86_instr(ag, X86_SETCC, i);

  cmp->v.binary.dst = operand_from_tac_val(i->src1);
  cmp->v.binary.src = new_x86_imm(0);

  mov->v.binary.src = new_x86_imm(0);
  sete->v.setcc.op = mov->v.binary.dst = operand_from_tac_val(i->dst);

  sete->v.setcc.cc = CC_E;
}

static void gen_asm_from_cpy_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  mov->v.binary.src = operand_from_tac_val(i->src1);
  mov->v.binary.dst = operand_from_tac_val(i->dst);
}

static void gen_asm_from_comparing_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *cmp = insert_x86_instr(ag, X86_CMP, i);
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  x86_instr *setcc = insert_x86_instr(ag, X86_SETCC, i);

  cmp->v.binary.dst = operand_from_tac_val(i->src1);
  cmp->v.binary.src = operand_from_tac_val(i->src2);

  mov->v.binary.src = new_x86_imm(0);

  setcc->v.setcc.op = mov->v.binary.dst = operand_from_tac_val(i->dst);

  int cc;
  switch (i->op) {
  case TAC_EQ:
    cc = CC_E;
    break;
  case TAC_NE:
    cc = CC_NE;
    break;
  case TAC_LT:
    cc = CC_L;
    break;
  case TAC_LE:
    cc = CC_LE;
    break;
  case TAC_GT:
    cc = CC_G;
    break;
  case TAC_GE:
    cc = CC_GE;
    break;
  default:
    UNREACHABLE();
  }

  setcc->v.setcc.cc = cc;
}

static void gen_asm_from_jump_instr(x86_asm_gen *ag, taci *i) {
  if (i->op == TAC_JMP) {
    x86_instr *res = insert_x86_instr(ag, X86_JMP, i);
    res->v.label = i->label_idx;
    return;
  }

  x86_instr *cmp = insert_x86_instr(ag, X86_CMP, i);
  x86_instr *jcc = insert_x86_instr(ag, X86_JMPCC, i);

  cmp->v.binary.src = new_x86_imm(0);
  cmp->v.binary.dst = operand_from_tac_val(i->src1);

  jcc->v.jmpcc.cc = i->op == TAC_JZ ? CC_E : CC_NE;
  jcc->v.jmpcc.label_idx = i->label_idx;
}

static void gen_asm_from_label_instr(x86_asm_gen *ag, taci *i) {
  insert_x86_instr(ag, X86_LABEL, i)->v.label = i->label_idx;
}

static void gen_asm_from_inc_dec(x86_asm_gen *ag, taci *i) {
  x86_instr *instr =
      insert_x86_instr(ag, i->op == TAC_INC ? X86_INC : X86_DEC, i);

  instr->v.unary.src = operand_from_tac_val(i->src1);
}

static void gen_asm_from_assign(x86_asm_gen *ag, taci *i) {
  if (i->op == TAC_ASDIV || i->op == TAC_ASMOD) {
    x86_instr *mov1 = insert_x86_instr(ag, X86_MOV, i);
    insert_x86_instr(ag, X86_CDQ, i);
    x86_instr *idiv = insert_x86_instr(ag, X86_IDIV, i);
    x86_instr *mov2 = insert_x86_instr(ag, X86_MOV, i);

    mov1->v.binary.dst = new_x86_reg(X86_AX);
    mov2->v.binary.dst = mov1->v.binary.src = operand_from_tac_val(i->dst);

    idiv->v.unary.src = operand_from_tac_val(i->src1);

    mov2->v.binary.src = new_x86_reg(i->op == TAC_ASDIV ? X86_AX : X86_DX);
    return;
  }

  int op;

  switch (i->op) {
  case TAC_ASADD:
    op = X86_ADD;
    break;
  case TAC_ASSUB:
    op = X86_SUB;
    break;
  case TAC_ASMUL:
    op = X86_MULT;
    break;
  case TAC_ASAND:
    op = X86_AND;
    break;
  case TAC_ASOR:
    op = X86_OR;
    break;
  case TAC_ASXOR:
    op = X86_XOR;
    break;
  case TAC_ASLSHIFT:
    op = X86_SHL;
    break;
  case TAC_ASRSHIFT:
    op = X86_SAR;
    break;
  default:
    UNREACHABLE();
  }

  x86_instr *instr = insert_x86_instr(ag, op, i);

  instr->v.binary.dst = operand_from_tac_val(i->dst);
  instr->v.binary.src = operand_from_tac_val(i->src1);
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
  case TAC_ADD:
  case TAC_SUB:
  case TAC_MUL:
  case TAC_DIV:
  case TAC_MOD:
  case TAC_AND:
  case TAC_OR:
  case TAC_XOR:
  case TAC_LSHIFT:
  case TAC_RSHIFT:
    gen_asm_from_binary_instr(ag, i);
    break;
  case TAC_NOT:
    gen_asm_from_not_instr(ag, i);
    break;
  case TAC_CPY:
    gen_asm_from_cpy_instr(ag, i);
    break;
  case TAC_EQ:
  case TAC_NE:
  case TAC_LT:
  case TAC_LE:
  case TAC_GT:
  case TAC_GE:
    gen_asm_from_comparing_instr(ag, i);
    break;
  case TAC_JMP:
  case TAC_JZ:
  case TAC_JNZ:
    gen_asm_from_jump_instr(ag, i);
    break;
  case TAC_LABEL:
    gen_asm_from_label_instr(ag, i);
    break;
  case TAC_INC:
  case TAC_DEC:
    gen_asm_from_inc_dec(ag, i);
    break;
  case TAC_ASADD:
  case TAC_ASSUB:
  case TAC_ASMUL:
  case TAC_ASDIV:
  case TAC_ASMOD:
  case TAC_ASAND:
  case TAC_ASOR:
  case TAC_ASXOR:
  case TAC_ASLSHIFT:
  case TAC_ASRSHIFT:
    gen_asm_from_assign(ag, i);
    break;
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

    i->next = res->first;
    res->first = i;
    i->next->prev = i;

    i->v.bytes_to_alloc = fix_pseudo_for_func(ag, res);
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
