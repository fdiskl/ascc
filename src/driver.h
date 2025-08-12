#ifndef _ASCC_DRIVER_H
#define _ASCC_DRIVER_H

#include "arena.h"
#include "common.h"

typedef struct _driver_options driver_options;

enum {          // (dof - Driver Option Flag)
  DOF_INVALID,  // represents invariant
  DOF_LEX,      // stop after lexer                      | --lex
  DOF_PARSE,    // stop after parser                     | --parse
  DOF_VALIDATE, // stop after sema                       | --validate, --sema
  DOF_CODEGEN,  // stop after TAC codegen                | --codegen, --tac
  DOF_S,        // stop after asm gen (emits .s file)    | -S, -s
  DOF_C,        // stop after assembling (emits .o file) | -c, -C
  DOF_ALL,      // do full pipeline
};

struct _driver_options {
  int dof; // driver option flag (enum)

  const char *output; // output file name, NULL if output file is not specified
                      // | -o <name>, --output <name>
  const char *input;  // input file path
};

void parse_driver_options(driver_options *d, int argc, char *argv[]);

/*
 *
 * For errors
 *
 */

// files to close before exiting
extern FILE **files_to_close;
// len of array 'files_to_close'
extern size_t files_to_close_len;

// files to delete before exiting
extern char **files_to_delete;
// len of array 'files_to_delete'
extern size_t files_to_delete_len;

// arenas to free
extern arena **arenas_to_free;
// len of array 'arenas_to_free'
extern size_t arenas_to_free_len;

// arenas to destroy
extern arena **arenas_to_destroy;
// len of array 'arenas_to_destory'
extern size_t arenas_to_destroy_len;

// function that should be invoked after any fatal error
// to free arenas, close/delete files etc
// (WILL terminate program)
void after_error();

// same as after_error, but WONT terminate program
void after_success();

#endif
