
#include "x86.h"

int max_offset;

// TODO: register allocation, graph coloring etc.
static void fix_pseudo_op(x86_op *op) {
  if (op->t != X86_OP_PSEUDO)
    return;

  op->t = X86_OP_STACK;
  op->v.stack_offset =
      (op->v.pseudo_idx + 1) * 4; // will do for now, hash table later mb

  if (op->v.stack_offset > max_offset)
    max_offset = op->v.stack_offset;
}

static void fix_pseudo_binary(x86_instr *i) {
  fix_pseudo_op(&i->v.binary.src);
  fix_pseudo_op(&i->v.binary.dst);
}

static void fix_pseudo_unary(x86_instr *i) { fix_pseudo_op(&i->v.unary.src); }

static void fix_pseudo_for_instr(x86_instr *i) {
  switch (i->op) {
  case X86_NOT:
  case X86_NEG:
  case X86_IDIV:
    fix_pseudo_unary(i);
    break;
  case X86_MOV:
  case X86_ADD:
  case X86_SUB:
  case X86_MULT:
  case X86_AND:
  case X86_OR:
  case X86_XOR:
  case X86_SHL:
  case X86_SAR:
    fix_pseudo_binary(i);
    break;
  case X86_RET:
  case X86_ALLOC_STACK:
  case X86_CDQ:
    break;
  }
}

int fix_pseudo_for_func(x86_func *f) {
  max_offset = 0;

  for (x86_instr *i = f->first; i != NULL; i = i->next)
    fix_pseudo_for_instr(i);

  return max_offset;
}
