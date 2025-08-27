#ifndef _ASCC_DRIVER_H
#define _ASCC_DRIVER_H

#include "arena.h"
#include "common.h"
#include "table.h"
#include "vec.h"

typedef struct _driver_options driver_options;

enum {          // (dof - Driver Option Flag)
  DOF_INVALID,  // represents invariant
  DOF_LEX,      // stop after lexer                      | --lex
  DOF_PARSE,    // stop after parser                     | --parse
  DOF_VALIDATE, // stop after sema                       | --validate, --sema
  DOF_TAC,      // stop after tac                        | --tacky, --tac
  DOF_CODEGEN,  // stop after asm codegen                | --codegen
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

// TODO: its still is a good idea to free arenas where they are not needed.

// vectors below are initialized in parse_driver_options func

VEC_T(files_to_close_t, FILE *);
extern files_to_close_t files_to_close;

VEC_T(files_to_delete_t, char *);
extern files_to_delete_t files_to_delete;

VEC_T(arenas_to_free_t, arena *);
extern arenas_to_free_t arenas_to_free;

VEC_T(arenas_to_destroy_t, arena *);
extern arenas_to_destroy_t arenas_to_destroy;

VEC_T(tables_to_destroy_t, ht *);
extern tables_to_destroy_t tables_to_destroy;

// function that should be invoked after any fatal error
// to free arenas, close/delete files etc
// (WILL terminate program)
void after_error();

// same as after_error, but WONT terminate program
void after_success();

#endif
