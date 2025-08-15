#include "common.h"
#include "x86.h"
#include <stdio.h>

static void emit_x86_reg(FILE *w, x86_reg reg, int size) {
  switch (size) {
  case 4:
    switch (reg) {
    case X86_AX:
      fprintf(w, "%%rax");
      break;
    default:
      todo();
    }
    break;
  default:
    todo();
  }
}

static void emit_x86_op(FILE *w, x86_op op) {
  switch (op.t) {
  case X86_OP_IMM:
    fprintf(w, "$%lu", op.v.imm);
    break;
  case X86_OP_REG:
    emit_x86_reg(w, op.v.reg.t, op.v.reg.size);
    break;
  }
}

static void emit_x86_binary(FILE *w, x86_instr *i, const char *name) {
  fprintf(w, "\t%s ", name);
  emit_x86_op(w, i->v.binary.src);
  fprintf(w, ", ");
  emit_x86_op(w, i->v.binary.dst);
  fprintf(w, "\n");
}

static void emit_x86_instr(FILE *w, x86_instr *i) {
  switch (i->op) {
  case X86_RET:
    fprintf(w, "\tret\n");
    break;
  case X86_MOV:
    emit_x86_binary(w, i, "mov");
    break;
  }
}

static void emit_x86_func(FILE *w, x86_func *f) {
  fprintf(w, "\t.globl %s\n", f->name);
  fprintf(w, "%s:\n", f->name);

  for (x86_instr *i = f->first; i != NULL; i = i->next) {
    emit_x86_instr(w, i);
  }
}

void emit_x86(FILE *w, x86_func *f) {
  for (; f != NULL; f = f->next)
    emit_x86_func(w, f);

  fprintf(w, ".section .note.GNU-stack,\"\",@progbits\n");
}
