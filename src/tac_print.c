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
    break;
  }
}

static void print_val(tacv *v) {
  switch (v->t) {
  case TACV_CONST:
    printf("%llu", (unsigned long long)v->intv);
    break;
  case TACV_VAR:
    printf("v%d", v->var_idx);
    break;
  default:
    unreachable();
  }
}

static void print_unary(tacv *dst, tacv *src, const char *ops) {
  print_val(dst);
  printf(" = %s", ops);
  print_val(src);
}

static void print_binary(tacv *dst, tacv *src1, tacv *src2, const char *ops) {
  print_val(dst);
  printf(" = ");
  print_val(src1);
  printf(" %s ", ops);
  print_val(src2);
}

void print_tac_func(tacf *f) {
  printf("func %s:\n", f->name);

  for (taci *i = f->firsti; i != NULL; i = i->next) {
    printf("\t");

    switch (i->op) {
    case TAC_RET:
      printf("%s ", tacop_str(i->op));
      print_val(&i->src1);
      break;
    case TAC_COMPLEMENT:
    case TAC_NEGATE:
      print_unary(&i->dst, &i->src1, tacop_str(i->op));
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
      print_binary(&i->dst, &i->src1, &i->src2, tacop_str(i->op));
      break;
    }

    printf("\n");
  }
}

void print_tac(tacf *first) {
  for (tacf *f = first; f != NULL; f = f->next) {
    print_tac_func(f);
    printf("\n");
  }
}
