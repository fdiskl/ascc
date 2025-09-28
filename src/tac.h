#ifndef _ASCC_TAC_H
#define _ASCC_TAC_H

#include "arena.h"
#include "parser.h"
#include "typecheck.h"
#include <stdint.h>
typedef struct _tac_instr taci;
typedef struct _tac_val tacv;
typedef struct _tac_func tacf;
typedef struct _tac_static_var tac_static_var;

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
  TAC_JMP,   // jmp (no vals)
  TAC_JZ,    // jmp if not zero (src1)
  TAC_JNZ,   // jmp if zero (src1)
  TAC_JE,    // jmp if equal (src1, src2)
  TAC_LABEL, // label: (no vals)
  TAC_CALL,  // call
} tacop;

typedef enum {
  TACV_CONST,
  TACV_VAR,
} tacvt;

struct _tac_val {
  tacvt t;

  union {
    uint64_t intv;
    string var;
  } v;
};

struct _tac_instr {
  tacop op;

  tacv dst;

  union {
    struct {
      tacv src1; // used for single val instructions (like return)
      tacv src2;
    } s;

    struct {
      tacv *args;
      size_t args_len;
      string name;
      bool plt;
    } call;
  } v;

  int label_idx; // for jumps, labels

  taci *next;
};

typedef struct _tac_top_level tac_top_level;

struct _tac_static_var {
  string name;
  bool global;
  uint64_t v;
};

struct _tac_func {
  string name;
  bool global;
  string *params;
  size_t params_len;
  taci *firsti;
};

struct _tac_top_level {
  bool is_func;
  union {
    tac_static_var v;
    tacf f;
  } v;
  tac_top_level *next;
};

struct _tacgen {
  arena taci_arena;
  arena tac_top_level_arena;
  arena tacv_arena;

  sym_table st;

  taci *head; // head of instr linked list of curr func
  taci *tail; // tail of instr linked list of curr func
};

void init_tacgen(tacgen *tg, sym_table st);
tac_top_level *gen_tac(tacgen *tg, program *p);

void print_tac(tac_top_level *first);
void fprint_taci(FILE *f, taci *i);
const char *tacop_str(tacop op);

#endif
