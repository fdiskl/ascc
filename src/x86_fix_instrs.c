
#include "common.h"
#include "x86.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static bool is_mem(int t) { return t == X86_OP_STACK || t == X86_OP_DATA; }

static void fix_instr(x86_asm_gen *ag, x86_instr *i);

static x86_op new_r10() {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg = X86_R10;
  return op;
}

static x86_op new_r11() {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg = X86_R11;
  return op;
}

static x86_op new_cx() {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg = X86_CX;
  return op;
}

// defined in x86.c
x86_instr *alloc_x86_instr(x86_asm_gen *ag, int op);

static void insert_before_x86_instr(x86_asm_gen *ag, x86_instr *i,
                                    x86_instr *new) {

  if (i->prev == NULL)
    UNREACHABLE(); // at least alloc_stack shoud be prev

  new->origin = i->origin;

  new->prev = i->prev;
  new->next = i;

  i->prev->next = new;
  i->prev = new;

  fix_instr(ag, new);
}

static void insert_after_x86_instr(x86_asm_gen *ag, x86_instr *i,
                                   x86_instr *new) {

  new->origin = i->origin;
  fix_instr(ag, new);

  new->prev = i;
  new->next = i->next;

  if (i->next)
    i->next->prev = new;

  i->next = new;
}

static void fix_unary_too_big_const(x86_asm_gen *ag, x86_instr *i) {
  if (i->v.unary.src.t == X86_OP_IMM && i->v.unary.src.v.imm > INT32_MAX) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.src = i->v.unary.src;
    i->v.unary.src = mov->v.binary.dst = new_r10();
    mov->v.binary.type = i->v.unary.type;
    insert_before_x86_instr(ag, i, mov);
  }
}

static void fix_binary_too_big_const(x86_asm_gen *ag, x86_instr *i) {
  if (i->v.binary.src.t == X86_OP_IMM && i->v.binary.src.v.imm > INT32_MAX) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.src = i->v.binary.src;
    i->v.binary.src = mov->v.binary.dst = new_r10();
    mov->v.binary.type = i->v.binary.type;
    insert_before_x86_instr(ag, i, mov);
  }
}

static void fix_both_ops_mem(x86_asm_gen *ag, x86_instr *i) {
  if (is_mem(i->v.binary.dst.t) && is_mem(i->v.binary.src.t)) {

    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.src = i->v.binary.src;
    i->v.binary.src = mov->v.binary.dst = new_r10();
    mov->v.binary.type = i->v.binary.type;

    insert_before_x86_instr(ag, i, mov);
  }
}

static void fix_idiv(x86_asm_gen *ag, x86_instr *i) {
  assert(i->op == X86_IDIV);

  if (i->v.unary.src.t == X86_OP_IMM) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.src = i->v.unary.src;
    i->v.unary.src = mov->v.binary.dst = new_r11();
    mov->v.binary.type = i->v.unary.type;
    insert_before_x86_instr(ag, i, mov);
  }
}

static void fix_mult(x86_asm_gen *ag, x86_instr *i) {
  assert(i->op == X86_MULT);

  if (is_mem(i->v.binary.dst.t)) {
    x86_instr *mov_before = alloc_x86_instr(ag, X86_MOV);
    x86_instr *mov_after = alloc_x86_instr(ag, X86_MOV);

    mov_before->v.binary.type = i->v.binary.type;
    mov_after->v.binary.type = i->v.binary.type;

    mov_after->v.binary.dst = mov_before->v.binary.src = i->v.binary.dst;
    i->v.binary.dst = mov_after->v.binary.src = mov_before->v.binary.dst =
        new_r11();

    insert_before_x86_instr(ag, i, mov_before);
    insert_after_x86_instr(ag, i, mov_after);
  }

  fix_binary_too_big_const(ag, i);
}

static void fix_shifts(x86_asm_gen *ag, x86_instr *i) {
  assert(i->op == X86_SAR || i->op == X86_SHL);

  // if not imm or cl
  if (i->v.binary.src.t != X86_OP_IMM &&
      !(i->v.binary.src.t == X86_OP_REG && i->v.binary.src.v.reg == X86_CX)) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.src = i->v.binary.src;
    mov->v.binary.dst = new_cx();
    mov->v.binary.type = i->v.binary.type;

    i->v.binary.src = new_cx();
    insert_before_x86_instr(ag, i, mov);
  }
}

static void fix_cmp(x86_asm_gen *ag, x86_instr *i) {
  fix_binary_too_big_const(ag, i);
  fix_both_ops_mem(ag, i);

  if (i->v.binary.dst.t == X86_OP_IMM) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.type = i->v.binary.type;
    insert_before_x86_instr(ag, i, mov);
    mov->v.binary.src = i->v.binary.dst;
    i->v.binary.dst = mov->v.binary.dst = new_r11();
  }
}

static void fix_movsx(x86_asm_gen *ag, x86_instr *i) {
  // also works when both ops are invalid

  if (i->v.binary.src.t == X86_OP_IMM) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    insert_before_x86_instr(ag, i, mov);
    mov->v.binary.src = i->v.binary.src;
    i->v.binary.src = mov->v.binary.dst = new_r10();
    mov->v.binary.type = X86_LONGWORD;
  }

  if (is_mem(i->v.binary.dst.t)) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    insert_after_x86_instr(ag, i, mov);
    mov->v.binary.dst = i->v.binary.dst;
    mov->v.binary.src = i->v.binary.dst = new_r11();
    mov->v.binary.type = X86_QUADWORD;
  }
}

static void fix_add_sub(x86_asm_gen *ag, x86_instr *i) {
  fix_both_ops_mem(ag, i);
  fix_binary_too_big_const(ag, i);
}

static void fix_mov(x86_asm_gen *ag, x86_instr *i) {
  fix_both_ops_mem(ag, i);

  if (i->v.binary.src.t == X86_OP_IMM && i->v.binary.src.v.imm > INT32_MAX &&
      i->v.binary.type == X86_LONGWORD) {
    i->v.binary.src.v.imm &= 0xffffffff;
  }

  if (i->v.binary.src.t == X86_OP_IMM && i->v.binary.src.v.imm > INT32_MAX &&
      i->v.binary.dst.t != X86_OP_REG) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    insert_before_x86_instr(ag, i, mov);
    mov->v.binary.src = i->v.binary.src;
    mov->v.binary.dst = i->v.binary.src = new_r10();
    mov->v.binary.type = i->v.binary.type;
  }
}

static void fix_instr(x86_asm_gen *ag, x86_instr *i) {
  switch (i->op) {
  case X86_RET:
  case X86_NOT:
  case X86_NEG:
  case X86_CDQ:
  case X86_JMP:
  case X86_JMPCC:
  case X86_SETCC:
  case X86_LABEL:
  case X86_COMMENT:
  case X86_INC:
  case X86_DEC:
  case X86_CALL:
    break;
  case X86_PUSH:
    fix_unary_too_big_const(ag, i);
    break;
  case X86_ADD:
  case X86_SUB:
    fix_add_sub(ag, i);
    break;
  case X86_AND:
  case X86_OR:
  case X86_XOR:
    fix_both_ops_mem(ag, i);
    fix_binary_too_big_const(ag, i);
    break;
  case X86_MOV:
    fix_mov(ag, i);
    break;
  case X86_IDIV:
    fix_idiv(ag, i);
    break;
  case X86_MULT:
    fix_mult(ag, i);
    break;
  case X86_SHL:
  case X86_SAR:
    fix_shifts(ag, i);
    break;
  case X86_CMP:
    fix_cmp(ag, i);
    break;
  case X86_MOVSX:
    fix_movsx(ag, i);
    break;
  }
}

void fix_instructions_for_func(x86_asm_gen *ag, x86_func *f) {
  for (x86_instr *i = f->first; i != NULL; i = i->next)
    fix_instr(ag, i);
}
