#ifndef _ASCC_COMMON_H
#define _ASCC_COMMON_H

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// If "DEBUG_INFO" is defined, compiler will emit extra debug info
#define DEBUG_INFO

// If "AST_PRINT_LOC" is defined AST printer will emit location info
#define AST_PRINT_LOC

// If "AST_PRINT_FILENAME_LOC" is defined AST printer will emit filename
// location info too
#define AST_PRINT_FILENAME_LOC

// i feel like it's too much info, so undef
#undef AST_PRINT_FILENAME_LOC

// If "ASM_DONT_FIX_PSEUDO" is defined pseudo-regs won't be replaced. Will
// result in not compiling asm, but useful for debugging
// Automatically will enable ASM_DONT_FIX_INSTRUCTIONS
#define ASM_DONT_FIX_PSEUDO
#undef ASM_DONT_FIX_PSEUDO

// If "ASM_DONT_FIX_INSTRUCTIONS" is defined invalid instructions won't be
// replaced. Will result in not compiling asm, but useful for debugging
#define ASM_DONT_FIX_INSTRUCTIONS
#undef ASM_DONT_FIX_INSTRUCTIONS

#define unreachable()                                                          \
  fprintf(stderr, "unreachable code reached (file: %s, line: %d)\n", __FILE__, \
          __LINE__);                                                           \
  after_error();                                                               \
  exit(1);

#define todo()                                                                 \
  fprintf(stderr, "not implemented yet (file: %s, line %d)\n", __FILE__,       \
          __LINE__);                                                           \
  after_error();                                                               \
  exit(1);

void after_error();

#endif
