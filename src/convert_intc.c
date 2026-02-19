#include "common.h"
#include "parser.h"
#include "type.h"
#include "typecheck.h"
#include <assert.h>

initial_init const_to_initial_double(double_const original) {
  initial_init res;
  res.t = INITIAL_DOUBLE;
  res.dv = original.v;
  return res;
}

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

double_const convert_const_to_double(int_const *original_ptr,
                                     double_const *d_original_ptr,
                                     type *convert_to) {
  if (convert_to->t != TYPE_DOUBLE) {
      int * a = NULL;
      int b = *a;
      UNREACHABLE();
  }
  assert((original_ptr || d_original_ptr) && !(original_ptr && d_original_ptr));

  double_const res;
  if (original_ptr) {
    res.v = (double)original_ptr->v;
  } else if (d_original_ptr) {
    res.v = (double)d_original_ptr->v;
  } else

    UNREACHABLE();

  return res;
}

int_const convert_const_to_int(int_const *original_ptr,
                               double_const *d_original_ptr, type *convert_to) {
  int_const res;
  assert((original_ptr || d_original_ptr) && !(original_ptr && d_original_ptr));

  if (original_ptr != NULL) {
    int_const original = *original_ptr;

    switch (convert_to->t) {
    case TYPE_INT: {
      res.t = CONST_INT;

      uint64_t v = (uint64_t)original.v;
      v &= 0xFFFFFFFFu;
      res.v = (int32_t)v;

      return res;
    }

    case TYPE_UINT: {
      res.t = CONST_UINT;
      res.v = (uint32_t)original.v;
      return res;
    }

    case TYPE_LONG: {
      res.t = CONST_LONG;
      res.v = (int64_t)original.v;
      return res;
    }

    case TYPE_ULONG: {
      res.t = CONST_ULONG;
      res.v = (uint64_t)original.v;
      return res;
    }

    case TYPE_DOUBLE:
      UNREACHABLE();

    case TYPE_FN:
      UNREACHABLE();
    }
  } else if (d_original_ptr != NULL) {
    double_const original = *d_original_ptr;

    switch (convert_to->t) {
    case TYPE_INT: {
      res.t = CONST_INT;

      res.v = (int32_t)original.v;

      return res;
    }

    case TYPE_UINT: {
      res.t = CONST_UINT;
      res.v = (uint32_t)original.v;
      return res;
    }

    case TYPE_LONG: {
      res.t = CONST_LONG;
      res.v = (int64_t)original.v;
      return res;
    }

    case TYPE_ULONG: {
      res.t = CONST_ULONG;
      res.v = (uint64_t)original.v;
      return res;
    }

    case TYPE_DOUBLE:
      UNREACHABLE();

    case TYPE_FN:
      UNREACHABLE();
    }
  }

  UNREACHABLE();
}
