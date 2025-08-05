#ifndef _ASCC_COMMON_H
#define _ASCC_COMMON_H

#include "stdio.h"

// If "DEBUG_INFO" is defined, compiler will emit extra debug info
#define DEBUG_INFO

#define unreachable()                                                          \
  fprintf(stderr, "unreachable code reached (file: %d, line: %d)", __FILE__,   \
          __LINE__)

#endif
