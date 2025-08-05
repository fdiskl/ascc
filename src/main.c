#include "arena.h"
#define extern_
#include "globals.h"
#undef extern_

int main(int argc, char *argv[]) {
  init_arena(&str_arena);
  free_arena(&str_arena);
}
