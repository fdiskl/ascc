#ifndef _ASCC_STRING_H
#define _ASCC_STRING_H

// read-only string, should not be modified after creation
#include "arena.h"
typedef char *string;

// creates new string from C-like string given
string new_string(const char *s);

#endif
