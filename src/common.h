#ifndef _ASCC_COMMON_H
#define _ASCC_COMMON_H

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// If "DEBUG_INFO" is defined, compiler will emit extra debug info
#define DEBUG_INFO

#define unreachable()                                                          \
  fprintf(stderr, "unreachable code reached (file: %s, line: %d)", __FILE__,   \
          __LINE__);                                                           \
  after_error();                                                               \
  exit(1);

#define todo()                                                                 \
  fprintf(stderr, "not implemented yet (file: %s, line %d)", __FILE__,         \
          __LINE__);                                                           \
  after_error();                                                               \
  exit(1);

void after_error();

#endif
