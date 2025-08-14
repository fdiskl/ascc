#include "tac.h"

const char *tacop_str(tacop op) {
  switch (op) {
  case TAC_RET:
    return "ret";
  default:
    unreachable();
  }
}

static void print_val(tacv *v) {
  switch (v->t) {
  case TACV_CONST:
    printf("#%llu", (unsigned long long)v->intv);
    break;
  case TACV_VAR:
    printf("v%d", v->var_idx);
    break;
  default:
    unreachable();
  }
}

void print_tac_func(tacf *f) {
  printf("func %s:\n", f->name);

  for (taci *i = f->firsti; i != NULL; i = i->next) {
    printf("    %-6s ", tacop_str(i->op));

    switch (i->op) {
    case TAC_RET:
      print_val(&i->src1);
      break;
    default:
      unreachable();
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
