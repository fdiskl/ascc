#include "arena.h"
#include "driver.h"
#include "scan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
  init_arena(&str_arena);

  driver_options opts;
  parse_driver_options(&opts, argc, argv);
  assert(strlen(opts.input) <= 255 && "input file name is too long");

  // create preprocessor file path (replace .c with .i)
  char preprocessor_file_path[256];
  {
    strcpy(preprocessor_file_path, opts.input);
    char *dot = strrchr(preprocessor_file_path, '.');
    assert(dot != NULL);
    // replace extension
    sprintf(dot, ".i");

#ifdef DEBUG_INFO
    printf("preprocessed file path: %s\n", preprocessor_file_path);
#endif
  }

  // run preprocessor
  {
    char cmd[1024];
    sprintf(cmd, "gcc -E %s -o %s", opts.input, preprocessor_file_path);

    int status = system(cmd);

    if (status == -1) {
      fprintf(stderr, "system failed");
      return 1;
    }

    if (WIFEXITED(status)) {
      int exit_code = WEXITSTATUS(status);
      if (exit_code != 0) {
        fprintf(stderr, "gcc preprocessor failed with exit code %d\n",
                exit_code);
        return exit_code;
      }
    } else if (WIFSIGNALED(status)) {
      fprintf(stderr, "gcc preprocessor terminated by signal %d\n",
              WTERMSIG(status));
      return 1;
    }
  }

  FILE *in_file = fopen(preprocessor_file_path, "r");

  lexer l;
  init_lexer(&l, in_file);

  if (opts.dof == DOF_LEX) {
    token t;
    do {
      next(&l, &t);
      print_token(&t);
    } while (t.token != TOK_EOF);

    remove(preprocessor_file_path);
    return 0;
  }

  remove(preprocessor_file_path);

  assert(0 && "todo");

  free_lexer(&l);
  free_arena(&str_arena);
}
