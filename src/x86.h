#ifndef _ASCC_X86_H
#define _ASCC_X86_H

#include "arena.h"
#include "common.h"
#include "strings.h"
#include "tac.h"
#include "type.h"
#include "typecheck.h"
#include <stdint.h>
typedef struct _x86_instr x86_instr;
typedef struct _x86_asm_gen x86_asm_gen;
typedef struct _x86_op x86_op;
typedef struct _x86_func x86_func;
typedef struct _x86_static_var x86_static_var;
typedef struct _x86_top_level x86_top_level;
typedef struct _x86_static_init x86_static_init;
typedef struct _x86_static_const x86_static_const;

// Automatically enable ASM_DONT_FIX_INSTRUCTIONS if ASM_DONT_FIX_PSEUDO is
// enabled. (see common.h)
#ifdef ASM_DONT_FIX_PSEUDO
#define ASM_DONT_FIX_INSTRUCTIONS
#endif

#ifndef PRINT_TAC_ORIGIN_X86
#ifdef PRINT_TAC_ORIGIN_X86_ONE_TIME
#undef PRINT_TAC_ORIGIN_X86_ONE_TIME
#endif
#endif

typedef enum {
  CC_E,
  CC_NE,
  CC_G,
  CC_GE,
  CC_L,
  CC_LE,
  CC_A,
  CC_AE,
  CC_B,
  CC_BE,
} x86_cc;

typedef enum {
  X86_AX,
  X86_CX,
  X86_DX,
  X86_DI,
  X86_SI,
  X86_R8,
  X86_R9,
  X86_R10,
  X86_R11,
  X86_SP,
  X86_XMM0,
  X86_XMM1,
  X86_XMM2,
  X86_XMM3,
  X86_XMM4,
  X86_XMM5,
  X86_XMM6,
  X86_XMM7,
  X86_XMM14,
  X86_XMM15,
} x86_reg;

typedef enum {
  X86_OP_IMM,
  X86_OP_REG,
  X86_OP_PSEUDO,
  X86_OP_STACK,
  X86_OP_DATA,
} x86_op_t;

typedef enum {
  // 0 operands
  X86_RET,
  X86_CDQ,

  // unary
  X86_NOT,
  X86_NEG,
  X86_IDIV,
  X86_DIV,
  X86_INC,
  X86_DEC,
  X86_PUSH,

  // binary
  X86_MOV,
  X86_ADD,
  X86_SUB,
  X86_MULT,
  X86_AND,
  X86_OR,
  X86_XOR,
  X86_SHL,
  X86_SHR,
  X86_SAR,
  X86_CMP,
  X86_MOVSX,
  X86_MOVZEXT,
  X86_CVTTSD2SI,
  X86_CVTSI2SD,
  X86_DIV_DOUBLE,

  // special
  X86_JMP,
  X86_JMPCC,
  X86_SETCC,
  X86_LABEL,
  X86_CALL,

  X86_COMMENT,
} x86_t;

typedef enum {
  X86_BYTE,
  X86_LONGWORD,
  X86_QUADWORD,
  X86_DOUBLE,
} x86_asm_type;

struct _x86_op {
  x86_op_t t;
  union {
    uint64_t imm;
    string pseudo;
    string data;
    int stack_offset;
    x86_reg reg;
  } v;
};

struct _x86_instr {
  x86_t op;
  union {
    struct {
      x86_op dst;
      x86_op src;
      x86_asm_type type;
    } binary;
    struct {
      x86_op src;
      x86_asm_type type;
    } unary;
    struct {
      x86_cc cc;
      int label_idx;
    } jmpcc;
    struct {
      x86_cc cc;
      x86_op op;
    } setcc;
    int label; // label or jump
    struct {
      char plt;
      string str_label; // call
    } call;
    struct {
      x86_asm_type type;
    } cdq;
    string comment; // for comment instr
  } v;
  x86_instr *next;
  x86_instr *prev;
  taci *origin; // NULL if not present
};

struct _x86_func {
  string name;
  x86_instr *first;
  x86_func *next;
  bool global;
};

struct _x86_static_var {
  string name;
  bool global;
  initial_init init;
  int alignment;
};

typedef enum {
  X86_STATIC_INIT_DOUBLE,
} x86_static_init_t;

struct _x86_static_init {
  x86_static_init_t t;
  union {
    double d;
  } v;
};

struct _x86_static_const {
  string name;
  int alignment;
  x86_static_init init;
};

typedef enum {
  X86_TL_FUNC,
  X86_TL_VAR,
  X86_TL_CONST,
} x86_top_level_t;

struct _x86_top_level {
  x86_top_level_t t;
  union {
    x86_func f;
    x86_static_var v;
    x86_static_const c;
  } v;
  x86_top_level *next;
};

struct _x86_asm_gen {
  arena *instr_arena;
  arena *top_level_arena;

  sym_table *st;

  x86_instr *head; // head of instr linked list for curr func
  x86_instr *tail; // tail of instr linked list for curr func
};

typedef struct _x86_program x86_program;
struct _x86_program {
  arena *instr_arena;     // will be freed by free_x86_program
  arena *top_level_arena; // will be freed by free_x86_program
  arena *be_syme_arena;   // will be freed by free_x86_program
  x86_top_level *first;
  ht *be_st; // will be destoyed by free_x86_program
};

typedef struct _be_syme be_syme;

typedef enum {
  BE_SYME_OBJ,
  BE_SYME_FN,
} be_syme_t;

struct _be_syme {
  be_syme_t t;
  union {
    struct {
      x86_asm_type type;
      bool is_static;
      bool is_const;
    } obj;
    struct {
      bool defined;
    } fn;
  } v;
};

x86_program gen_asm(tac_program *tac_prog, sym_table *st);
void emit_be_st(ht *be_st);

void free_x86_program(x86_program *p);

// replaces pseudo instructions, is called by gen_asm
// returns amount of bytes to be allocated for locals
int fix_pseudo_for_func(x86_asm_gen *ag, x86_func *f, ht *bst);

// fixes invalid instructions, is called by gen_asm
void fix_instructions_for_func(x86_asm_gen *ag, x86_func *f);

void emit_x86(FILE *w, x86_program *prog);

#endif
