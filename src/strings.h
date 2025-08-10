#ifndef _ASCC_STRING_H
#define _ASCC_STRING_H

#include "arena.h"

extern arena str_arena;

// read-only string, should not be modified after creation
typedef char *string;

// creates new string from C-like string given
string new_string(const char *s);

#endif
