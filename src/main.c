#include "arena.h"
#include "common.h"
#include "driver.h"
#include "parser.h"
#include "scan.h"
#include "strings.h"
#include "tac.h"
#include "typecheck.h"
#include "x86.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

double now_seconds();

arena ptr_arena; // arena to allocate pointers (void*)

static void replace_ext(const char *original, char *dst, const char *ext);

int main(int argc, char *argv[]) {
#ifdef DEBUG_INFO
  double start = now_seconds();
#endif

  INIT_ARENA(&str_arena, char);
  vec_push_back(arenas_to_free, &str_arena);

  INIT_ARENA(&ptr_arena, void *);
  vec_push_back(arenas_to_free, &ptr_arena);

  driver_options opts;
  parse_driver_options(&opts, argc, argv);
  assert(strlen(opts.input) <= 255 && "input file name is too long");

  // create some file names
  char preprocessor_file_path[256];
  replace_ext(opts.input, preprocessor_file_path, ".i");
#ifdef DEBUG_INFO
  printf("preprocessed file path: %s\n", preprocessor_file_path);
#endif

  char output_file_path[256];
  if (opts.output == NULL) {
    replace_ext(opts.input, output_file_path, "");
    opts.output = output_file_path;
  }

#ifdef DEBUG_INFO
  printf("output file path: %s\n", opts.output);
#endif

  char asm_file_path[256];
  replace_ext(opts.input, asm_file_path, ".s");

#ifdef DEBUG_INFO
  printf("asm file path: %s\n", asm_file_path);
#endif

  // run preprocessor
  {
    char cmd[1024];
    sprintf(cmd, "gcc -E %s -o %s", opts.input, preprocessor_file_path);

    int status = system(cmd);

#ifdef _WIN32
    int exit_code = status;
    if (exit_code != 0) {
      fprintf(stderr, "gcc preprocessor failed with exit code %d\n", exit_code);
      after_error();
      return exit_code;
    }
#else
    if (status == -1) {
      fprintf(stderr, "system failed");
      after_error();
      return 1;
    }

    if (WIFEXITED(status)) {
      int exit_code = WEXITSTATUS(status);
      if (exit_code != 0) {
        fprintf(stderr, "gcc preprocessor failed with exit code %d\n",
                exit_code);
        after_error();
        return exit_code;
      }
    } else if (WIFSIGNALED(status)) {
      fprintf(stderr, "gcc preprocessor terminated by signal %d\n",
              WTERMSIG(status));
      after_error();
      return 1;
    }
#endif
  }

  FILE *in_file = fopen(preprocessor_file_path, "r");

  vec_push_back(files_to_close, in_file);
  vec_push_back(files_to_delete, preprocessor_file_path);

  lexer l;
  init_lexer(&l, in_file);

  if (opts.dof == DOF_LEX) {
    token t;
    do {
      next(&l, &t);
      print_token(&t);
    } while (t.token != TOK_EOF);

    after_success();
    return 0;
  }

  parser p;
  init_parser(&p, &l);

  program *parsed_ast = parse(&p);

  if (opts.dof == DOF_PARSE) {
    print_program(parsed_ast);
    after_success();
    return 0;
  }

  sym_table st = typecheck(parsed_ast);

  if (opts.dof == DOF_VALIDATE) {
    print_program(parsed_ast);
    printf("\n");
    print_sym_table(st);
    after_success();
    return 0;
  }

  tacgen tg;
  init_tacgen(&tg);

  tacf *tac_prog = gen_tac(&tg, parsed_ast);

  if (opts.dof == DOF_TAC) {
    print_tac(tac_prog);
    after_success();
    return 0;
  }

  x86_asm_gen ag;
  init_x86_asm_gen(&ag, st);

  x86_func *x86_prog = gen_asm(&ag, tac_prog);

  if (opts.dof == DOF_CODEGEN) {
    // TODO: print mb
    after_success();
    return 0;
  }

  FILE *asm_file = fopen(asm_file_path, "w");
  // NOTE: we dont add this file to cleanup array files to close and do it
  // mannualy, because it should be done before assembler

  emit_x86(asm_file, x86_prog);
  fclose(asm_file);

#ifdef DEBUG_INFO
  printf("-------asm  res-------\n");
  emit_x86(stdout, x86_prog);
  printf("----------------------\n");
#endif

  if (opts.dof == DOF_S) {
    after_success();
    return 0;
  }

  vec_push_back(files_to_delete, asm_file_path);

  if (opts.dof == DOF_C) {
    TODO();
  };

  // run assembler
  {

    char cmd[1024];
    sprintf(cmd, "gcc %s -o %s", asm_file_path, opts.output);

    int status = system(cmd);

#ifdef _WIN32
    int exit_code = status;
    if (exit_code != 0) {
      fprintf(stderr, "gas (using gcc) failed with exit code %d\n", exit_code);
      after_error();
      return exit_code;
    }
#else
    if (status == -1) {
      fprintf(stderr, "system failed");
      after_error();
      return 1;
    }

    if (WIFEXITED(status)) {
      int exit_code = WEXITSTATUS(status);
      if (exit_code != 0) {
        fprintf(stderr, "gas (using gcc) failed with exit code %d\n",
                exit_code);
        after_error();
        return exit_code;
      }
    } else if (WIFSIGNALED(status)) {
      fprintf(stderr, "gas (using gcc) terminated by signal %d\n",
              WTERMSIG(status));
      after_error();
      return 1;
    }

#endif
  }

#ifdef DEBUG_INFO
  double end = now_seconds();
  printf("Done in %.9f seconds\n", end - start);
#endif

  after_success();
  return 0;
}

static void replace_ext(const char *original, char *dst, const char *ext) {
  strcpy(dst, original);
  char *dot = strrchr(dst, '.');
  assert(dot != NULL);
  // replace extension
  sprintf(dot, "%s", ext);
}

// add now_seconds utility
#ifdef _WIN32
#include <windows.h>

double now_seconds() {
  static LARGE_INTEGER freq;
  static int initialized = 0;
  if (!initialized) {
    QueryPerformanceFrequency(&freq);
    initialized = 1;
  }
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
#include <time.h>
double now_seconds() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
#endif
