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

// TODO: it still is a good idea to free arenas where they are not needed. Add
// REMOVE_FROM_CLEANUP_ARRAY macro

/* NOTE 1:
* we could add last element indexes for 4 arrays below (files_to_close,
files_to_delete, arenas_to_free, arenas_to_destroy), but because this arrs are
really small (<100 elements) - it is just easier to go through and find first
NULL element
*/

/* NOTE 2:
* 4 arrays below (files_to_close,
files_to_delete, arenas_to_free, arenas_to_destroy) are initialized with NULL's
in 'parse_driver_options' function, because i dont want separate func for it
 */

// len for next 4 arrays (files_to_close, files_to_delete, arenas_to_free,
// arenas_to_destroy)
#define ARRAY_FOR_CLEANUP_LEN 32

// files to close before exiting (NULL for empty 'slots')
extern FILE *files_to_close[ARRAY_FOR_CLEANUP_LEN];
// files to delete before exiting (NULL for empty 'slots')
extern char *files_to_delete[ARRAY_FOR_CLEANUP_LEN];
// arenas to free (NULL for empty 'slots')
extern arena *arenas_to_free[ARRAY_FOR_CLEANUP_LEN];
// arenas to destroy (NULL for empty 'slots')
extern arena *arenas_to_destroy[ARRAY_FOR_CLEANUP_LEN];

#define ADD_TO_CLEANUP_ARRAY(arr, new_element)                                 \
  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)                              \
    if (arr[i] == NULL) {                                                      \
      arr[i] = new_element;                                                    \
      break;                                                                   \
    }

// function that should be invoked after any fatal error
// to free arenas, close/delete files etc
// (WILL terminate program)
void after_error();

// same as after_error, but WONT terminate program
void after_success();

#endif
