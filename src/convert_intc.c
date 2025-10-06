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
  }

  res.v = original.v;

  return res;
}

int_const convert_const(int_const original, type *convert_to) {
  int_const res;
  switch (convert_to->t) {
  case TYPE_INT:
    res.t = CONST_INT;
    if (original.t == CONST_INT) {
      res.v = original.v;
      return res;
    } else if (original.t == CONST_LONG) {

      // as defined in implementation defined behaviour, we subtract 2^32
      // (drop 4 upper bytes)
      res.v = original.v & 0xFFFFFFFF;
      return res;
    } else
      UNREACHABLE();
  case TYPE_LONG:
    res.t = CONST_LONG;
    res.v = original.v;
    return res;
  case TYPE_FN:
    UNREACHABLE();
    break;
  }
}
