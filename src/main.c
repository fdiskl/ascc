#include "arena.h"
#include "driver.h"
#include "scan.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  init_arena(&str_arena);

  driver_options opts;

  parse_driver_options(&opts, argc, argv);

  FILE *in_file = fopen(opts.input, "r");

  lexer l;
  init_lexer(&l, in_file);

  if (opts.dof == DOF_LEX) {
    token t;
    do {
      next(&l, &t);
      print_token(&t);
    } while (t.token != TOK_EOF);
    return 0;
  }

  assert(0 && "todo");

  free_lexer(&l);
  free_arena(&str_arena);
}
