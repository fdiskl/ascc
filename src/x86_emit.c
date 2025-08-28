#include "common.h"
#include "tac.h"
#include "x86.h"
#include <stdio.h>

static void emit_x86_reg(FILE *w, x86_reg reg, int size) {

  switch (size) {
  case 4: {
    switch (reg) {
    case X86_AX:
      fprintf(w, "%%eax");
      return;
    case X86_DX:
      fprintf(w, "%%edx");
      return;
    case X86_CX:
      fprintf(w, "%%ecx");
      return;
    case X86_R10:
      fprintf(w, "%%r10d");
      return;
    case X86_R11:
      fprintf(w, "%%r11d");
      return;
    }
  } break;

  case 1: {
    switch (reg) {
    case X86_AX:
      fprintf(w, "%%al");
      return;
    case X86_DX:
      fprintf(w, "%%dl");
      return;
    case X86_CX:
      fprintf(w, "%%cl");
      return;
    case X86_R10:
      fprintf(w, "%%r10b");
      return;
    case X86_R11:
      fprintf(w, "%%r11b");
      return;
    }
  } break;
  }

  TODO();
}

static void emit_origin(FILE *w, x86_instr *i) {
  fprintf(w, "\t");
#ifdef PRINT_TAC_ORIGIN_X86
  if (i->origin != NULL)
    fprint_taci(w, i->origin);
#endif
  fprintf(w, "\n");
}

static void emit_x86_op(FILE *w, x86_op op, int size) {
  switch (op.t) {
  case X86_OP_IMM:
    fprintf(w, "$%lu", op.v.imm);
    break;
  case X86_OP_REG:
    emit_x86_reg(w, op.v.reg, size);
    break;
  case X86_OP_PSEUDO:
    fprintf(w, "PSEUDO(%d)", op.v.pseudo_idx);
    break;
  case X86_OP_STACK:
    fprintf(w, "-%d(%%rbp)", op.v.stack_offset);
    break;
  }
}

static void emit_x86_unary(FILE *w, x86_instr *i, const char *name) {
  fprintf(w, "\t%s ", name);
  emit_x86_op(w, i->v.unary.src, 4);
  emit_origin(w, i);
}

static void emit_x86_binary(FILE *w, x86_instr *i, const char *name) {
  fprintf(w, "\t%s ", name);
  emit_x86_op(w, i->v.binary.src, 4);
  fprintf(w, ", ");
  emit_x86_op(w, i->v.binary.dst, 4);
  emit_origin(w, i);
}

static void emit_x86_shift(FILE *w, x86_instr *i, const char *name) {
  fprintf(w, "\t%s ", name);
  // it should be cx, so this little hack if you can call it that
  if (i->v.binary.src.t == X86_OP_REG && i->v.binary.src.v.reg == X86_CX)
    emit_x86_op(w, i->v.binary.src, 1);
  else
    emit_x86_op(w, i->v.binary.src, 1);
  fprintf(w, ", ");
  emit_x86_op(w, i->v.binary.dst, 4);
  emit_origin(w, i);
}

static const char *cc_code(x86_cc cc) {
  switch (cc) {
  case CC_E:
    return "e";
  case CC_NE:
    return "ne";
  case CC_G:
    return "g";
  case CC_GE:
    return "ge";
  case CC_L:
    return "l";
  case CC_LE:
    return "le";
    break;
  }

  UNREACHABLE();
}

static void emit_x86_instr(FILE *w, x86_instr *i) {
  switch (i->op) {
  case X86_RET:
    fprintf(w, "\t# function epilogue\n");
    fprintf(w, "\tmovq %%rbp, %%rsp\n");
    fprintf(w, "\tpopq %%rbp\n");
    fprintf(w, "\tret\n");
    break;
  case X86_MOV:
    emit_x86_binary(w, i, "movl");
    break;
  case X86_ADD:
    emit_x86_binary(w, i, "addl");
    break;
  case X86_SUB:
    emit_x86_binary(w, i, "subl");
    break;
  case X86_MULT:
    emit_x86_binary(w, i, "imull");
    break;
  case X86_AND:
    emit_x86_binary(w, i, "andl");
    break;
  case X86_OR:
    emit_x86_binary(w, i, "orl");
    break;
  case X86_XOR:
    emit_x86_binary(w, i, "xorl");
    break;
  case X86_SHL:
    emit_x86_shift(w, i, "shll");
    break;
  case X86_SAR:
    emit_x86_shift(w, i, "sarl");
    break;
  case X86_CMP:
    emit_x86_binary(w, i, "cmpl");
    break;
  case X86_NOT:
    emit_x86_unary(w, i, "notl");
    break;
  case X86_NEG:
    emit_x86_unary(w, i, "negl");
    break;
  case X86_IDIV:
    emit_x86_unary(w, i, "idivl");
    break;
  case X86_ALLOC_STACK:
    fprintf(w, "\tsubq $%d, %%rsp", i->v.bytes_to_alloc);
    emit_origin(w, i);
    break;
  case X86_CDQ:
    fprintf(w, "\tcdq");
    emit_origin(w, i);
    break;
  case X86_JMP:
    fprintf(w, "\tjmp .L%d", i->v.label);
    emit_origin(w, i);
    break;
  case X86_JMPCC:
    fprintf(w, "\tj%s .L%d", cc_code(i->v.jmpcc.cc), i->v.jmpcc.label_idx);
    emit_origin(w, i);
    break;
  case X86_SETCC:
    fprintf(w, "\tset%s ", cc_code(i->v.setcc.cc));
    emit_x86_op(w, i->v.setcc.op, 1);
    emit_origin(w, i);
    break;
  case X86_LABEL:
    fprintf(w, "\t.L%d:", i->v.label);
    emit_origin(w, i);
    break;
  }
}

static void emit_x86_func(FILE *w, x86_func *f) {
  fprintf(w, "\t.globl %s\n", f->name);
  fprintf(w, "%s:\n", f->name);
  fprintf(w, "\tpushq %%rbp\n");
  fprintf(w, "\tmovq %%rsp, %%rbp\n");

  for (x86_instr *i = f->first; i != NULL; i = i->next) {
    emit_x86_instr(w, i);
  }
}

void emit_x86(FILE *w, x86_func *f) {
  for (; f != NULL; f = f->next)
    emit_x86_func(w, f);

  fprintf(w, ".section .note.GNU-stack,\"\",@progbits\n");
}
