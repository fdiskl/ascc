#include "common.h"
#include "tac.h"
#include "x86.h"
#include <stdio.h>

#ifdef PRINT_TAC_ORIGIN_X86_ONE_TIME
#define SMART_EMIT_ORIGIN(code)                                                \
  do {                                                                         \
    emit_origin(w, i);                                                         \
    code;                                                                      \
  } while (0)
#endif

#ifndef PRINT_TAC_ORIGIN_X86_ONE_TIME
#define SMART_EMIT_ORIGIN(code)                                                \
  do {                                                                         \
    code;                                                                      \
    emit_origin(w, i);                                                         \
  } while (0)
#endif

static char get_suff(x86_asm_type t) {
  switch (t) {
  case X86_LONGWORD:
    return 'l';
  case X86_QUADWORD:
    return 'q';
  case X86_BYTE:
    return 'b';
  }

  UNREACHABLE();
}

static void emit_x86_reg(FILE *w, x86_reg reg, x86_asm_type t) {
  switch (t) {
  case X86_QUADWORD: {
    switch (reg) {
    case X86_AX:
      fprintf(w, "%%rax");
      return;
    case X86_DX:
      fprintf(w, "%%rdx");
      return;
    case X86_CX:
      fprintf(w, "%%rcx");
      return;
    case X86_DI:
      fprintf(w, "%%rdi");
      return;
    case X86_SI:
      fprintf(w, "%%rsi");
      return;
    case X86_R8:
      fprintf(w, "%%r8");
      return;
    case X86_R9:
      fprintf(w, "%%r9");
      return;
    case X86_R10:
      fprintf(w, "%%r10");
      return;
    case X86_R11:
      fprintf(w, "%%r11");
      return;
    case X86_SP:
      fprintf(w, "%%rsp");
      return;
    }
    break;
  }
  case X86_LONGWORD: {
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
    case X86_DI:
      fprintf(w, "%%edi");
      return;
    case X86_SI:
      fprintf(w, "%%esi");
      return;
    case X86_R8:
      fprintf(w, "%%r8d");
      return;
    case X86_R9:
      fprintf(w, "%%r9d");
      return;
    case X86_R10:
      fprintf(w, "%%r10d");
      return;
    case X86_R11:
      fprintf(w, "%%r11d");
      return;
    case X86_SP:
      fprintf(w, "%%esp");
      return;
    }
    break;
  }

  case X86_BYTE: {
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
    case X86_DI:
      fprintf(w, "%%dil");
      return;
    case X86_SI:
      fprintf(w, "%%sil");
      return;
    case X86_R8:
      fprintf(w, "%%r8b");
      return;
    case X86_R9:
      fprintf(w, "%%r9b");
      return;
    case X86_R10:
      fprintf(w, "%%r10b");
      return;
    case X86_R11:
      fprintf(w, "%%r11b");
      return;
    case X86_SP:
      fprintf(w, "%%spl");
      return;
    }
    break;
  }
  }

  TODO();
}

static taci *last_origin = NULL;

static void emit_origin(FILE *w, x86_instr *i) {
  if (i->origin == NULL) {
    fprintf(w, "\n");
    return;
  }
  fprintf(w, "\t");
#ifdef PRINT_TAC_ORIGIN_X86
#ifdef PRINT_TAC_ORIGIN_X86_ONE_TIME
  if (i->origin != last_origin) {
    fprintf(w, "\n");
    fprintf(w, "\n\t# ");
    fprint_taci(w, i->origin);
    last_origin = i->origin;
  }
#else
  fprintf(w, "# ");
  fprint_taci(w, i->origin);
#endif
#endif
  fprintf(w, "\n");
}

static void emit_x86_op(FILE *w, x86_op op, x86_asm_type t) {
  switch (op.t) {
  case X86_OP_IMM:
    fprintf(w, "$%lu", op.v.imm);
    break;
  case X86_OP_REG:
    emit_x86_reg(w, op.v.reg, t);
    break;
  case X86_OP_PSEUDO:
    fprintf(w, "PSEUDO(%s)", op.v.pseudo);
    break;
  case X86_OP_STACK:
    if (op.v.stack_offset > 0)
      fprintf(w, "-%d(%%rbp)", op.v.stack_offset);
    else
      fprintf(w, "%d(%%rbp)", -op.v.stack_offset);
    break;
  case X86_OP_DATA:
    fprintf(w, "%s(%%rip)", op.v.data);
    break;
  }
}

static void emit_x86_unary(FILE *w, x86_instr *i, const char *name) {
  SMART_EMIT_ORIGIN({
    fprintf(w, "\t%s%c ", name, get_suff(i->v.unary.type));
    emit_x86_op(w, i->v.unary.src, i->v.unary.type);
  });
}

static void emit_x86_binary(FILE *w, x86_instr *i, const char *name) {
  SMART_EMIT_ORIGIN({
    fprintf(w, "\t%s%c ", name, get_suff(i->v.binary.type));
    emit_x86_op(w, i->v.binary.src, i->v.binary.type);
    fprintf(w, ", ");
    emit_x86_op(w, i->v.binary.dst, i->v.binary.type);
  });
}

static void emit_x86_shift(FILE *w, x86_instr *i, const char *name) {
  SMART_EMIT_ORIGIN({
    fprintf(w, "\t%s%c ", name, get_suff(i->v.binary.type));
    emit_x86_op(w, i->v.binary.src, X86_BYTE);
    fprintf(w, ", ");
    emit_x86_op(w, i->v.binary.dst, i->v.binary.type);
  });
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
  case CC_A:
    return "a";
  case CC_AE:
    return "ae";
  case CC_B:
    return "b";
  case CC_BE:
    return "be";
    break;
  }

  UNREACHABLE();
}

static void emit_x86_instr(FILE *w, x86_instr *i) {
  switch (i->op) {
  case X86_RET:
    fprintf(w, "\n\tmovq %%rbp, %%rsp\n");
    fprintf(w, "\tpopq %%rbp\n");
    fprintf(w, "\tret\n");
    break;
  case X86_MOV:
    emit_x86_binary(w, i, "mov");
    break;
  case X86_ADD:
    emit_x86_binary(w, i, "add");
    break;
  case X86_SUB:
    emit_x86_binary(w, i, "sub");
    break;
  case X86_MULT:
    emit_x86_binary(w, i, "imul");
    break;
  case X86_AND:
    emit_x86_binary(w, i, "and");
    break;
  case X86_OR:
    emit_x86_binary(w, i, "or");
    break;
  case X86_XOR:
    emit_x86_binary(w, i, "xor");
    break;
  case X86_SHL:
    emit_x86_shift(w, i, "shl");
    break;
  case X86_SAR:
    emit_x86_shift(w, i, "sar");
    break;
  case X86_SHR:
    emit_x86_shift(w, i, "shr");
    break;
  case X86_CMP:
    emit_x86_binary(w, i, "cmp");
    break;
  case X86_NOT:
    emit_x86_unary(w, i, "not");
    break;
  case X86_NEG:
    emit_x86_unary(w, i, "neg");
    break;
  case X86_IDIV:
    emit_x86_unary(w, i, "idiv");
    break;
  case X86_DIV:
    emit_x86_unary(w, i, "div");
    break;
  case X86_INC:
    emit_x86_unary(w, i, "inc");
    break;
  case X86_DEC:
    emit_x86_unary(w, i, "dec");
    break;
  case X86_PUSH:
    emit_x86_unary(w, i, "push");
    break;
  case X86_CDQ:
    SMART_EMIT_ORIGIN(
        fprintf(w, "\t%s", i->v.cdq.type == X86_QUADWORD ? "cqo" : "cdq"););

    break;
  case X86_JMP:
    SMART_EMIT_ORIGIN(fprintf(w, "\tjmp .L%d", i->v.label));
    break;
  case X86_JMPCC:
    SMART_EMIT_ORIGIN(fprintf(w, "\tj%s .L%d", cc_code(i->v.jmpcc.cc),
                              i->v.jmpcc.label_idx););
    break;
  case X86_SETCC:
    SMART_EMIT_ORIGIN(fprintf(w, "\tset%s ", cc_code(i->v.setcc.cc));
                      emit_x86_op(w, i->v.setcc.op, 1););
    break;
  case X86_LABEL:
    SMART_EMIT_ORIGIN(fprintf(w, "\t.L%d:", i->v.label););
    break;
  case X86_COMMENT:
    fprintf(w, "\t#%s\n", i->v.comment);
    break;
  case X86_CALL:
    if (i->v.call.plt)
      SMART_EMIT_ORIGIN(fprintf(w, "\tcall %s@plt\n", i->v.call.str_label););
    else
      SMART_EMIT_ORIGIN(fprintf(w, "\tcall %s\n", i->v.call.str_label););
    break;
  case X86_MOVSX:
    SMART_EMIT_ORIGIN({
      fprintf(w, "\tmovslq ");
      emit_x86_op(w, i->v.binary.src, X86_LONGWORD);
      fprintf(w, ", ");
      emit_x86_op(w, i->v.binary.dst, X86_QUADWORD);
    });
    break;
  case X86_MOVZEXT:
    UNREACHABLE();
    break;
  }
}

static void emit_x86_global(FILE *w, bool global, string name) {
  if (global) {
    fprintf(w, "\t.globl %s\n", name);
  }
}

static void emit_x86_func(FILE *w, x86_func *f) {
  fprintf(w, "# Start of function %s\n", f->name);
  emit_x86_global(w, f->global, f->name);
  fprintf(w, "\t.text\n");
  fprintf(w, "%s:\n", f->name);
  fprintf(w, "\t# func prologue \n");
  fprintf(w, "\tpushq %%rbp\n");
  fprintf(w, "\tmovq %%rsp, %%rbp\n\n");

  for (x86_instr *i = f->first; i != NULL; i = i->next) {
    emit_x86_instr(w, i);
  }

  fprintf(w, "# End of function %s\n\n", f->name);
}

static void emit_x86_static_var(FILE *w, x86_static_var *sv) {
  emit_x86_global(w, sv->global, sv->name);
  if (sv->init.v == 0)
    fprintf(w, "\t.bss\n");
  else
    fprintf(w, "\t.data\n");

  fprintf(w, "\t.balign 4\n");
  fprintf(w, "%s:\n", sv->name);

  switch (sv->init.t) {
  case INITIAL_INT:
  case INITIAL_UINT:
    if (sv->init.v == 0) {
      fprintf(w, "\t.zero 4\n");
    } else {
      fprintf(w, "\t.long %llu\n", (unsigned long long)sv->init.v);
    }
    break;
  case INITIAL_LONG:
  case INITIAL_ULONG:
    if (sv->init.v == 0) {
      fprintf(w, "\t.zero 8\n");
    } else {
      fprintf(w, "\t.quad %llu\n", (unsigned long long)sv->init.v);
    }
    break;
  }
}

void emit_x86(FILE *w, x86_program *prog) {
  for (x86_top_level *tl = prog->first; tl != NULL; tl = tl->next)
    if (tl->is_func)
      emit_x86_func(w, &tl->v.f);
    else
      emit_x86_static_var(w, &tl->v.v);

#ifndef _WIN32
  fprintf(w, ".section .note.GNU-stack,\"\",@progbits\n");
#endif
}
