
#include "common.h"
#include "x86.h"
#include <assert.h>

static x86_op new_r10() {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg.t = X86_R10;
  op.v.reg.size = 4;
  return op;
}

static x86_op new_r11() {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg.t = X86_R11;
  op.v.reg.size = 4;
  return op;
}

// defined in x86.c
x86_instr *alloc_x86_instr(x86_asm_gen *ag, int op);

static void insert_before_x86_instr(x86_instr *i, x86_instr *new) {

  if (i->prev == NULL)
    unreachable(); // at least alloc_stack shoud be prev

  new->next = i;
  i->prev->next = new;
}

static void insert_after_x86_instr(x86_instr *i, x86_instr *new) {
  new->next = i->next;
  i->next = new;
}

static void fix_mov_add_sub(x86_asm_gen *ag, x86_instr *i) {
  assert(i->op == X86_MOV || i->op == X86_ADD || i->op == X86_SUB);

  if (i->v.binary.dst.t == X86_OP_STACK && i->v.binary.src.t == X86_OP_STACK) {

    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.src = i->v.binary.src;
    i->v.binary.src = mov->v.binary.dst = new_r10();

    insert_before_x86_instr(i, mov);
  }
}

static void fix_idiv(x86_asm_gen *ag, x86_instr *i) {
  assert(i->op == X86_IDIV);

  if (i->v.unary.src.t == X86_OP_IMM) {
    x86_instr *mov = alloc_x86_instr(ag, X86_MOV);
    mov->v.binary.src = i->v.unary.src;
    i->v.unary.src = mov->v.binary.dst = new_r11();
    insert_before_x86_instr(i, mov);
  }
}

static void fix_mult(x86_asm_gen *ag, x86_instr *i) {
  assert(i->op == X86_MULT);

  if (i->v.binary.dst.t == X86_OP_STACK) {
    x86_instr *mov_before = alloc_x86_instr(ag, X86_MOV);
    x86_instr *mov_after = alloc_x86_instr(ag, X86_MOV);

    mov_after->v.binary.dst = mov_before->v.binary.src = i->v.binary.dst;
    i->v.binary.dst = mov_after->v.binary.src = mov_before->v.binary.dst =
        new_r11();

    insert_before_x86_instr(i, mov_before);
    insert_after_x86_instr(i, mov_after);
  }
}

static void fix_instr(x86_asm_gen *ag, x86_instr *i) {
  switch (i->op) {
  case X86_RET:
  case X86_NOT:
  case X86_NEG:
  case X86_CDQ:
  case X86_ALLOC_STACK:
    break;
  case X86_MOV:
  case X86_ADD:
  case X86_SUB:
    fix_mov_add_sub(ag, i);
    break;
  case X86_IDIV:
    fix_idiv(ag, i);
    break;
  case X86_MULT:
    fix_mult(ag, i);
    break;
  }
}

void fix_instructions_for_func(x86_asm_gen *ag, x86_func *f) {
  for (x86_instr *i = f->first; i != NULL; i = i->next)
    fix_instr(ag, i);
}
