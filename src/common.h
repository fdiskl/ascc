#ifndef _ASCC_COMMON_H
#define _ASCC_COMMON_H

#include "stdio.h"
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>

// If "DEBUG_INFO" is defined, compiler will emit extra debug info
#define DEBUG_INFO

#define unreachable()                                                          \
  fprintf(stderr, "unreachable code reached (file: %s, line: %d)", __FILE__,   \
          __LINE__)

#define todo()                                                                 \
  fprintf(stderr, "not implemented yet (file: %s, line %d)", __FILE__, __LINE__)

void after_error();

#endif
