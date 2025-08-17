
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

static void fix_mov(x86_asm_gen *ag, x86_instr *mov) {
  assert(mov->op == X86_MOV);

  if (mov->v.binary.dst.t == X86_OP_STACK &&
      mov->v.binary.src.t == X86_OP_STACK) {

    x86_instr *new_mov = alloc_x86_instr(ag, X86_MOV);
    new_mov->v.binary.src = new_r10();
    new_mov->v.binary.dst = mov->v.binary.dst;

    mov->v.binary.dst = new_r10();

    insert_after_x86_instr(mov, new_mov);
  }
}

static void fix_instr(x86_asm_gen *ag, x86_instr *i) {
  switch (i->op) {
  case X86_RET:
  case X86_NOT:
  case X86_NEG:
  case X86_ALLOC_STACK:
    break;
  case X86_MOV:
    fix_mov(ag, i);
    break;
  }
}

void fix_instructions_for_func(x86_asm_gen *ag, x86_func *f) {
  for (x86_instr *i = f->first; i != NULL; i = i->next)
    fix_instr(ag, i);
}
