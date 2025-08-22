#ifndef _ASCC_X86_H
#define _ASCC_X86_H

#include "arena.h"
#include "strings.h"
#include "tac.h"
typedef struct _x86_instr x86_instr;
typedef struct _x86_asm_gen x86_asm_gen;
typedef struct _x86_op x86_op;
typedef struct _x86_func x86_func;

// Automatically enable ASM_DONT_FIX_INSTRUCTIONS if ASM_DONT_FIX_PSEUDO is
// enabled. (see common.h)
#ifdef ASM_DONT_FIX_PSEUDO
#define ASM_DONT_FIX_INSTRUCTIONS
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
  X86_DX,
  X86_CX,
  X86_R10,
  X86_R11,
} x86_reg;

typedef enum {
  X86_OP_IMM,
  X86_OP_REG,
  X86_OP_PSEUDO,
  X86_OP_STACK,
} x86_op_t;

typedef enum {
  // 0 operands
  X86_RET,
  X86_CDQ,

  // unary
  X86_NOT,
  X86_NEG,
  X86_IDIV,

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
  X86_JMP,
  X86_JMPCC,
  X86_SETCC,
  X86_LABEL,
} x86_t;

struct _x86_op {
  x86_op_t t;
  union {
    uint64_t imm;
    int pseudo_idx;
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
    int bytes_to_alloc;
  } v;
  x86_instr *next;
  x86_instr *prev;
};

struct _x86_func {
  string name;
  x86_instr *first;
  x86_func *next;
};

struct _x86_asm_gen {
  arena instr_arena;
  arena func_arena;

  x86_instr *head; // head of instr linked list for curr func
  x86_instr *tail; // tail of instr linked list for curr func
};

void init_x86_asm_gen(x86_asm_gen *ag);

x86_func *gen_asm(x86_asm_gen *ag, tacf *tac_first_f);

// replaces pseudo instructions, is called by gen_asm
// returns amount of bytes to be allocated
int fix_pseudo_for_func(x86_func *f);

// fixes invalid instructions, is called by gen_asm
void fix_instructions_for_func(x86_asm_gen *ag, x86_func *f);

void emit_x86(FILE *w, x86_func *first_func);

#endif
