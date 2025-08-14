#ifndef _ASCC_TAC_H
#define _ASCC_TAC_H

#include <stdint.h>
typedef struct _tac_instr taci;
typedef struct _tac_val tacv;

enum {
  TAC_RET,
};

struct _tac_instr {
  int op;

  tacv *dst;
  tacv *src1;
  tacv *src2;
};

enum {
  TACV_CONST,
  TACV_VAR,
};

struct _tac_val {
  int t;

  union {
    uint64_t intv;
    int var_idx;
  };
};

#endif
