#include "arena.h"
#include "scan.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  init_arena(&str_arena);

  // TODO: driver
  FILE *in_file = fopen("test.c", "r");

  lexer l;
  init_lexer(&l, in_file);

  // TODO: only if --lex is set
  if (1) {
    token t;
    do {
      next(&l, &t);
      print_token(&t);
    } while (t.token != TOK_EOF);
  }

  free_lexer(&l);

  free_arena(&str_arena);
}
