#ifndef _ASCC_X86_H
#define _ASCC_X86_H

#include "arena.h"
#include "strings.h"
#include "tac.h"
typedef struct _x86_instr x86_instr;
typedef struct _x86_asm_gen x86_asm_gen;
typedef struct _x86_op x86_op;
typedef struct _x86_func x86_func;

typedef enum {
  X86_AX,
} x86_reg;

typedef enum {
  X86_OP_IMM,
  X86_OP_REG,
} x86_op_t;

typedef enum {
  // 0 operands
  X86_RET,

  // binary
  X86_MOV,
} x86_t;

struct _x86_op {
  x86_op_t t;
  union {
    uint64_t imm;
    struct {
      x86_reg t;
      int size;
    } reg;
  } v;
};

struct _x86_instr {
  x86_t op;
  union {
    struct {
      x86_op dst;
      x86_op src;
    } binary;
  } v;
  x86_instr *next;
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
void emit_x86(FILE *w, x86_func *first_func);

#endif
