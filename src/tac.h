#ifndef _ASCC_TAC_H
#define _ASCC_TAC_H

#include "arena.h"
#include "parser.h"
#include <stdint.h>
typedef struct _tac_instr taci;
typedef struct _tac_val tacv;
typedef struct _tac_func tacf;

typedef struct _tacgen tacgen;

typedef enum {
  // singe val
  TAC_RET, // return
  TAC_INC, // ++
  TAC_DEC, // --

  // unary
  TAC_NEGATE,     // -
  TAC_COMPLEMENT, // ~
  TAC_NOT,        // !
  TAC_CPY,        // a = b
  TAC_ASADD,      // +=
  TAC_ASSUB,      // -=
  TAC_ASMUL,      // *=
  TAC_ASDIV,      // /=
  TAC_ASMOD,      // %=
  TAC_ASAND,      // &=
  TAC_ASOR,       // |=
  TAC_ASXOR,      // ^=
  TAC_ASLSHIFT,   // >>=
  TAC_ASRSHIFT,   // <<=

  // binary
  TAC_ADD,    // +
  TAC_SUB,    // -
  TAC_MUL,    // *
  TAC_DIV,    // /
  TAC_MOD,    // %
  TAC_AND,    // &
  TAC_OR,     // |
  TAC_XOR,    // ^
  TAC_LSHIFT, // <<
  TAC_RSHIFT, // >>
  TAC_EQ,     // ==
  TAC_NE,     // !=
  TAC_LT,     // <
  TAC_LE,     // <=
  TAC_GT,     // >
  TAC_GE,     // >=

  // with labels
  TAC_JMP,   // jmp
  TAC_JZ,    // jmp if not zero
  TAC_JNZ,   // jmp if zero
  TAC_LABEL, // label:
} tacop;

typedef enum {
  TACV_CONST,
  TACV_VAR,
} tacvt;

struct _tac_val {
  tacvt t;

  union {
    uint64_t intv;
    int var_idx;
  } v;
};

struct _tac_instr {
  tacop op;

  tacv dst;
  tacv src1; // used for single val instructions (like return)
  tacv src2;

  int label_idx; // for jumps, labels

  taci *next;
};

struct _tac_func {
  string name;
  taci *firsti;

  tacf *next;
};

struct _tacgen {
  arena taci_arena;
  arena tacf_arena;

  taci *head; // head of instr linked list of curr func
  taci *tail; // tail of instr linked list of curr func
};

void init_tacgen(tacgen *tg);
tacf *gen_tac(tacgen *tg, program *p);

void print_tac(tacf *first);
void fprint_taci(FILE *f, taci *i);
const char *tacop_str(tacop op);

#endif
