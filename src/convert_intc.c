#include "common.h"
#include "parser.h"
#include "typecheck.h"

initial_init const_to_initial(int_const original) {
  initial_init res;
  switch (original.t) {
  case CONST_INT:
    res.t = INITIAL_INT;
    break;
  case CONST_LONG:
    res.t = INITIAL_LONG;
    break;
  case CONST_UINT:
    res.t = INITIAL_UINT;
    break;
  case CONST_ULONG:
    res.t = INITIAL_ULONG;
    break;
  }

  res.v = original.v;

  return res;
}

#define MASK32 ((uint64_t)0xFFFFFFFF)
#define MASK64 ((uint64_t)0xFFFFFFFFFFFFFFFF)

int_const convert_const(int_const original, type *convert_to) {
  int_const res;

  switch (convert_to->t) {

  case TYPE_INT:
    res.t = CONST_INT;
    // implementation-defined: drop upper 4 bytes (subtract 2^32)
    res.v = original.v & MASK32;
    return res;

  case TYPE_UINT:
    res.t = CONST_UINT;
    res.v = original.v & MASK32;
    return res;

  case TYPE_LONG:
    res.t = CONST_LONG;
    res.v = original.v & MASK64;
    return res;

  case TYPE_ULONG:
    res.t = CONST_ULONG;
    res.v = original.v & MASK64;
    return res;

  case TYPE_FN:
    UNREACHABLE();
  }

  UNREACHABLE();
}
