#include "common.h"
#include "tac.h"
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
  case TAC_CPY:
  case TAC_JMP:
  case TAC_JZ:
  case TAC_JNZ:
  case TAC_LABEL:
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
    fprintf(f, "v%d", v->v.var_idx);
    break;
  default:
    UNREACHABLE();
  }
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

void fprint_taci(FILE *f, taci *i) {
  switch (i->op) {
  case TAC_RET:
    fprintf(f, "%s ", tacop_str(i->op));
    fprint_val(f, &i->src1);
    break;
  case TAC_COMPLEMENT:
  case TAC_NEGATE:
  case TAC_NOT:
    fprint_unary(f, &i->dst, &i->src1, tacop_str(i->op));
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
    fprint_binary(f, &i->dst, &i->src1, &i->src2, tacop_str(i->op));
    break;
  case TAC_CPY:
    fprint_val(f, &i->dst);
    fprintf(f, " = ");
    fprint_val(f, &i->src1);
    break;
  case TAC_JMP:
    fprintf(f, "jmp -> L%d", i->label_idx);
    break;
  case TAC_JZ:
    fprintf(f, "jz ");
    fprint_val(f, &i->src1);
    fprintf(f, " -> L%d", i->label_idx);
    break;
  case TAC_JNZ:
    fprintf(f, "jnz ");
    fprint_val(f, &i->src1);
    fprintf(f, " -> L%d", i->label_idx);
    break;
  case TAC_LABEL:
    fprintf(f, "L%d:", i->label_idx);
    break;
  }
}

void print_tac_func(tacf *f) {
  printf("func %s:\n", f->name);

  for (taci *i = f->firsti; i != NULL; i = i->next) {
    printf("\t");
    fprint_taci(stdout, i);
    printf("\n");
  }
}

void print_tac(tacf *first) {
  for (tacf *f = first; f != NULL; f = f->next) {
    print_tac_func(f);
    printf("\n");
  }
}
