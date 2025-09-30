#include "arena.h"
#include "common.h"
#include "tac.h"
#include <stdint.h>
#include <stdio.h>

const char *tacop_str(tacop op) {
  switch (op) {
  case TAC_RET:
    return "ret";
  case TAC_COMPLEMENT:
    return "~";
  case TAC_NEGATE:
    return "-";
  case TAC_ADD:
    return "+";
  case TAC_SUB:
    return "-";
  case TAC_MUL:
    return "*";
  case TAC_DIV:
    return "/";
  case TAC_MOD:
    return "%";
  case TAC_AND:
    return "&";
  case TAC_OR:
    return "|";
  case TAC_XOR:
    return "^";
  case TAC_LSHIFT:
    return "<<";
  case TAC_RSHIFT:
    return ">>";
  case TAC_NOT:
    return "not";
  case TAC_EQ:
    return "eq";
  case TAC_NE:
    return "ne";
  case TAC_LT:
    return "lt";
  case TAC_LE:
    return "le";
  case TAC_GT:
    return "gt";
  case TAC_GE:
    return "ge";
  case TAC_INC:
    return "inc";
  case TAC_DEC:
    return "dec";
  case TAC_ASADD:
    return "+=";
  case TAC_ASSUB:
    return "-=";
  case TAC_ASMUL:
    return "*=";
  case TAC_ASDIV:
    return "/=";
  case TAC_ASMOD:
    return "%=";
  case TAC_ASAND:
    return "&=";
  case TAC_ASOR:
    return "|=";
  case TAC_ASXOR:
    return "^=";
  case TAC_ASLSHIFT:
    return "<<=";
  case TAC_ASRSHIFT:
    return ">>=";
  case TAC_CPY:
  case TAC_JMP:
  case TAC_JZ:
  case TAC_JNZ:
  case TAC_LABEL:
  case TAC_JE:
  case TAC_CALL:
    break;
  }

  UNREACHABLE();
}

static void fprint_val(FILE *f, tacv *v) {
  switch (v->t) {
  case TACV_CONST:
    fprintf(f, "%llu", (unsigned long long)v->v.intv);
    break;
  case TACV_VAR:
    fprintf(f, "%s", v->v.var);
    break;
  default:
    UNREACHABLE();
  }
}

static void fprint_assignment(FILE *f, tacv *dst, tacv *src, const char *ops) {
  fprint_val(f, dst);
  fprintf(f, " %s ", ops);
  fprint_val(f, src);
}

static void fprint_unary(FILE *f, tacv *dst, tacv *src, const char *ops) {
  fprint_val(f, dst);
  fprintf(f, " = %s ", ops);
  fprint_val(f, src);
}

static void fprint_binary(FILE *f, tacv *dst, tacv *src1, tacv *src2,
                          const char *ops) {
  fprint_val(f, dst);
  fprintf(f, " = ");
  fprint_val(f, src1);
  fprintf(f, " %s ", ops);
  fprint_val(f, src2);
}

static void fprint_single_val(FILE *f, tacv *src, const char *ops) {
  fprintf(f, "%s ", ops);
  fprint_val(f, src);
}

void fprint_taci(FILE *f, taci *i) {
  switch (i->op) {
  case TAC_RET:
  case TAC_INC:
  case TAC_DEC:
    fprint_single_val(f, &i->v.s.src1, tacop_str(i->op));
    break;
  case TAC_COMPLEMENT:
  case TAC_NEGATE:
  case TAC_NOT:
    fprint_unary(f, &i->dst, &i->v.s.src1, tacop_str(i->op));
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
    fprint_assignment(f, &i->dst, &i->v.s.src1, tacop_str(i->op));
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
  case TAC_EQ:
  case TAC_NE:
  case TAC_LT:
  case TAC_LE:
  case TAC_GT:
  case TAC_GE:
    fprint_binary(f, &i->dst, &i->v.s.src1, &i->v.s.src2, tacop_str(i->op));
    break;
  case TAC_CPY:
    fprint_val(f, &i->dst);
    fprintf(f, " = ");
    fprint_val(f, &i->v.s.src1);
    break;
  case TAC_JMP:
    fprintf(f, "jmp -> L%d", i->label_idx);
    break;
  case TAC_JZ:
    fprintf(f, "jz ");
    fprint_val(f, &i->v.s.src1);
    fprintf(f, " -> L%d", i->label_idx);
    break;
  case TAC_JNZ:
    fprintf(f, "jnz ");
    fprint_val(f, &i->v.s.src1);
    fprintf(f, " -> L%d", i->label_idx);
    break;
  case TAC_LABEL:
    fprintf(f, "L%d:", i->label_idx);
    break;
  case TAC_JE:
    fprintf(f, "je ");
    fprint_val(f, &i->v.s.src1);
    fprintf(f, " == ");
    fprint_val(f, &i->v.s.src2);
    fprintf(f, " -> L%d", i->label_idx);
    break;
  case TAC_CALL:
    fprint_val(f, &i->dst);
    fprintf(f, " = call%s %s(", i->v.call.plt ? "@plt" : "", i->v.call.name);
    if (i->v.call.args != NULL)
      fprint_val(f, &i->v.call.args[0]);
    for (int j = 1; j < i->v.call.args_len; ++j) {
      fprintf(f, ", ");
      fprint_val(f, &i->v.call.args[j]);
    }
    fprintf(f, ")\n");
    break;
  }
}

static void print_tac_func(tacf *f) {
  if (f->global)
    printf("global func %s(", f->name);
  else
    printf("func %s(", f->name);
  if (f->params != NULL) {
    printf("%s", f->params[0]);
    for (int i = 1; i < f->params_len; ++i)
      printf(", %s", f->params[i]);
  }
  printf("):\n");

  for (taci *i = f->firsti; i != NULL; i = i->next) {
    printf("\t");
    fprint_taci(stdout, i);
    printf("\n");
  }
}

static void print_tac_static_var(tac_static_var *sv) {
  if (sv->global)
    printf("global static %s = %llu", sv->name, (long long unsigned)sv->v);
  else
    printf("static %s = %llu", sv->name, (long long unsigned)sv->v);
}

void print_tac(tac_program *prog) {
  for (tac_top_level *tl = prog->first; tl != NULL; tl = tl->next) {
    if (tl->is_func) {
      print_tac_func(&tl->v.f);
    } else {
      print_tac_static_var(&tl->v.v);
    }
    printf("\n");
  }
}
