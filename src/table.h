#ifndef _ASCC_TABLE_H
#define _ASCC_TABLE_H

// https://benhoyt.com/writings/hash-table-in-c/

#include "common.h"

#define HT_INITIAL_CAPACITY 16

typedef struct ht ht;
typedef struct _hti hti;

typedef size_t *hash_fn(const ht *t, const void *key);
typedef int *eq_fn(const void *a, const void *b);

// TODO: swap for uintptr_t

ht *ht_create(void);
void ht_destroy(ht *t);

// ht has ability to work as linked list
ht *ht_get_next_table(ht *t);
void ht_set_next_table(ht *t, ht *next);

// Get item with given NULL-terminated key from hash table.
// Returns pointer to value or NULL if not found
void *ht_get(ht *table, const char *key);

// Set item with given key to given value. Key would be reallocated in area.
// Returns addr of copied key or NULL on failure.
const char *ht_set(ht *table, const char *key, void *value);

size_t ht_size(ht *table);

struct _hti {
  const char *key; // curr key
  void *value;     // curr value

  ht *_table;
  size_t _index;
};

// create new iterator
hti ht_iterator(ht *table);

// advance iterator, returns false when end is reached. Using ht_set while
// iterating will lead to undefined bahviour
bool ht_next(hti *it);

#endif
