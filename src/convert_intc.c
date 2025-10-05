#include "common.h"
#include "parser.h"
#include "typecheck.h"

initial_init convert_const(int_const original, type *convert_to) {
  initial_init res;
  switch (convert_to->t) {
  case TYPE_INT:
    res.t = INITIAL_INT;
    if (original.t == CONST_INT) {
      res.v = original.v;
      return res;
    } else if (original.t == CONST_LONG) {
      if (original.v > (1ULL << 31) - 1) { // > 2 ^ 31 - 1
        // as defined in implementation defined behaviour, we subtract 2^32
        // (drop 4 upper bytes)

        res.v = original.v & 0xFFFFFFFF; // drop first 4 bytes
      }
    } else
      UNREACHABLE();
  case TYPE_LONG:
    res.t = INITIAL_LONG;
    res.v = original.v;
    return res;
  case TYPE_FN:
    UNREACHABLE();
    break;
  }
}
