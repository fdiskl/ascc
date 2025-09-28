#ifndef _ASCC_X86_H
#define _ASCC_X86_H

#include "arena.h"
#include "strings.h"
#include "tac.h"
#include "typecheck.h"
#include <stdint.h>
typedef struct _x86_instr x86_instr;
typedef struct _x86_asm_gen x86_asm_gen;
typedef struct _x86_op x86_op;
typedef struct _x86_func x86_func;
typedef struct _x86_static_var x86_static_var;
typedef struct _x86_top_level x86_top_level;

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
  X86_SAR,
  X86_CMP,

  // special
  X86_ALLOC_STACK,
  X86_DEALLOC_STACK,
  X86_JMP,
  X86_JMPCC,
  X86_SETCC,
  X86_LABEL,
  X86_CALL,

  X86_COMMENT,
} x86_t;

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
    } binary;
    struct {
      x86_op src;
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
    int bytes_to_alloc;
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
  uint64_t v;
};

struct _x86_top_level {
  bool is_func;
  union {
    x86_func f;
    x86_static_var v;
  } v;
  x86_top_level *next;
};

struct _x86_asm_gen {
  arena instr_arena;
  arena top_level_arena;

  x86_instr *head; // head of instr linked list for curr func
  x86_instr *tail; // tail of instr linked list for curr func
};

void init_x86_asm_gen(x86_asm_gen *ag);

x86_top_level *gen_asm(x86_asm_gen *ag, tac_top_level *tac_first_top_level,
                       sym_table st);

// replaces pseudo instructions, is called by gen_asm
// returns amount of bytes to be allocated for locals
int fix_pseudo_for_func(x86_asm_gen *ag, x86_func *f, sym_table st);

// fixes invalid instructions, is called by gen_asm
void fix_instructions_for_func(x86_asm_gen *ag, x86_func *f);

void emit_x86(FILE *w, x86_top_level *first_top_level);

#endif
