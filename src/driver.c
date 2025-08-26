#include "driver.h"
#include "arena.h"
#include "common.h"
#include "table.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_driver_options(const driver_options *d);

#define SET_COMPILER_DOF(d, set_to)                                            \
  {                                                                            \
    if (d->dof != DOF_INVALID) {                                               \
      fprintf(stderr, "flag for compiler stage is already set\n");             \
      exit(1);                                                                 \
    }                                                                          \
    d->dof = set_to;                                                           \
    continue;                                                                  \
  }

#define OUTPUT_FLAG_ERR()                                                      \
  {                                                                            \
    fprintf(stderr, "output file already set\n");                              \
    exit(1);                                                                   \
  }

#define SET_OUTPUT_FLAG(d, next_arg_is_out)                                    \
  {                                                                            \
    if (d->output != NULL || next_arg_is_out)                                  \
      OUTPUT_FLAG_ERR();                                                       \
    next_arg_is_out = true;                                                    \
    continue;                                                                  \
  }

// todo: help

void parse_driver_options(driver_options *d, int argc, char *argv[]) {
  bool next_arg_is_out = false;
  d->dof = DOF_INVALID;
  d->output = NULL;
  d->input = NULL;

  // -- init arrays for cleanup (see NOTE 2 in driver.h)
  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    files_to_close[i] = NULL;

  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    files_to_delete[i] = NULL;

  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    arenas_to_free[i] = NULL;

  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    arenas_to_destroy[i] = NULL;

  // --

  // i=0 for program name :)
  for (int i = 1; i < argc; ++i) {
    assert(strlen(argv[i]) > 1);
    if (argv[i][0] == '-') { // flag
      if (next_arg_is_out) {
        fprintf(stderr, "expected outfile file name, found flag %s\n", argv[i]);
        exit(1);
      }
      if (argv[i][1] == '-') // long flag
        switch (argv[i][2]) {
        case 'o':
          if (!strcmp(argv[i], "--output")) // --output
            SET_OUTPUT_FLAG(d, next_arg_is_out);
          break;
        case 'l':
          if (!strcmp(argv[i], "--lex")) // --lex
            SET_COMPILER_DOF(d, DOF_LEX);
          break;
        case 'p':
          if (!strcmp(argv[i], "--parse")) // --parse
            SET_COMPILER_DOF(d, DOF_PARSE);
          break;
        case 'v':
          if (!strcmp(argv[i], "--validate")) // --validate
            SET_COMPILER_DOF(d, DOF_VALIDATE);
          break;

        case 's':
          if (!strcmp(argv[i], "--sema")) // --sema
            SET_COMPILER_DOF(d, DOF_VALIDATE);
          break;
        case 'c':
          if (!strcmp(argv[i], "--codegen")) // --codegen
            SET_COMPILER_DOF(d, DOF_CODEGEN);
          break;
        case 't':
          if (!strcmp(argv[i], "--tac")) // --tac
            SET_COMPILER_DOF(d, DOF_TAC);
          if (!strcmp(argv[i], "--tacky")) // --tacky
            SET_COMPILER_DOF(d, DOF_TAC);
          break;
        }
      else
        switch (argv[i][1]) {
        case 'o': // -o
          SET_OUTPUT_FLAG(d, next_arg_is_out);
          break;
        case 's':
        case 'S':
          SET_COMPILER_DOF(d, DOF_S);
          break;
        case 'c':
        case 'C':
          SET_COMPILER_DOF(d, DOF_C);
          break;
        }

      fprintf(stderr, "invalid flag %s\n", argv[i]);
      exit(1);
    } else { // args
      if (next_arg_is_out) {
        next_arg_is_out = false;
        if (d->output != NULL) {
          OUTPUT_FLAG_ERR();
        }

        d->output = argv[i];
        continue;
      }

      if (d->input != NULL) {
        fprintf(stderr, "input file already set\n");
        exit(1);
      }
      d->input = argv[i];
    }
  }

  if (d->dof == DOF_INVALID)
    d->dof = DOF_ALL;

  if (d->input == NULL) {
    fprintf(stderr, "input file is required\n");
    exit(1);
  }

#ifdef DEBUG_INFO
  print_driver_options(d);
#endif
}

static const char *dof_to_string(int dof) {
  switch (dof) {
  case DOF_INVALID:
    return "INVALID";
  case DOF_LEX:
    return "Lexing";
  case DOF_PARSE:
    return "Parsing";
  case DOF_VALIDATE:
    return "Validation";
  case DOF_CODEGEN:
    return "Code Generation";
  case DOF_S:
    return "Assembly (-S)";
  case DOF_C:
    return "Compile (-c)";
  case DOF_ALL:
    return "All (full pipeline)";
  default:
    return "Unknown";
  }
}

static void print_driver_options(const driver_options *d) {
  printf("--- Driver Options ---\n");
  printf("Input file : %s\n", d->input ? d->input : "(none)");
  printf("Output file: %s\n", d->output ? d->output : "(none)");
  printf("Stage      : %s\n", dof_to_string(d->dof));
  printf("----------------------\n");
}

void after_success() {
  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    if (files_to_close[i] != NULL)
      fclose(files_to_close[i]);

  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    if (files_to_delete[i] != NULL)
      remove(files_to_delete[i]);

  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    if (arenas_to_free[i] != NULL)
      free_arena(arenas_to_free[i]);

  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    if (arenas_to_destroy[i] != NULL)
      destroy_arena(arenas_to_destroy[i]);

  for (int i = 0; i < ARRAY_FOR_CLEANUP_LEN; ++i)
    if (tables_to_destroy[i] != NULL)
      ht_destroy(tables_to_destroy[i]);
}

void after_error() {
  after_success();
  exit(1);
}

FILE *files_to_close[ARRAY_FOR_CLEANUP_LEN];
char *files_to_delete[ARRAY_FOR_CLEANUP_LEN];
arena *arenas_to_free[ARRAY_FOR_CLEANUP_LEN];
arena *arenas_to_destroy[ARRAY_FOR_CLEANUP_LEN];
ht *tables_to_destroy[ARRAY_FOR_CLEANUP_LEN];
