
#include "table.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

typedef struct _hte hte;

struct _hte {
  const char *key;
  void *val;
};

struct ht {
  hte *entries;
  size_t cap;
  size_t size;
};

ht *ht_create(void) {
  ht *t = (ht *)malloc(sizeof(ht));

  assert(t);
  t->size = 0;
  t->cap = HT_INITIAL_CAPACITY;

  t->entries = calloc(t->cap, sizeof(hte));
  assert(t->entries);

  return t;
}

void ht_destroy(ht *t) {
  for (size_t i = 0; i < t->cap; ++i)
    free((void *)t->entries[i].key);

  free(t->entries);
  free(t);
}

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

// Return 64-bit FNV-1a hash for NULL terminated key.
// (https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function)
static uint64_t hash_key(const char *key) {
  uint64_t hash = FNV_OFFSET;
  for (const char *p = key; *p; ++p) {
    hash ^= (uint64_t)(unsigned char)(*p);
    hash *= FNV_PRIME;
  }
  return hash;
}

void *ht_get(ht *t, const char *key) {
  // AND hash with (cap - 1) so it's always within entries arr
  uint64_t hash = hash_key(key);
  size_t idx = (size_t)(hash & (uint64_t)(t->cap - 1));

  // linear probing
  while (t->entries[idx].key != NULL) {
    if (strcmp(key, t->entries[idx].key) == 0)
      return t->entries[idx].val;
    ++idx;
    if (idx >= t->cap)
      idx = 0; // wrap around
  }

  return NULL;
}

// Sets entry without expanding size
static const char *ht_set_entry(hte *entries, size_t cap, const char *key,
                                void *v, size_t *psize) {
  // AND hash with (cap - 1) so it's always within entries arr
  uint64_t hash = hash_key(key);
  size_t idx = (size_t)(hash & (uint64_t)(cap - 1));

  while (entries[idx].key != NULL) {
    if (strcmp(key, entries[idx].key) == 0) {
      entries[idx].val = v;
      return entries[idx].key;
    }

    ++idx;
    if (idx >= cap)
      idx = 0; // wrap around
  }

  // key not found
  if (psize != NULL) {
    key = strdup(key);
    assert(key);
    ++(*psize);
  }

  entries[idx].key = key;
  entries[idx].val = v;
  return key;
}

// twice size, true on success, false on failure
static bool ht_expand(ht *t) {
  size_t newcap = t->cap * 2;

  if (newcap < t->cap)
    return false;

  hte *new_entries = calloc(newcap, sizeof(hte));
  assert(new_entries);

  for (size_t i = 0; i < t->cap; ++i) {
    hte e = t->entries[i];
    if (e.key != NULL)
      ht_set_entry(new_entries, newcap, e.key, e.val, NULL);
  }

  free(t->entries);
  t->entries = new_entries;
  t->cap = newcap;
  return true;
}

const char *ht_set(ht *t, const char *key, void *v) {
  assert(v != NULL);

  // if 2*size >= cap => expand
  if (t->size >= t->cap / 2) {
    if (!ht_expand(t)) {
      return NULL;
    }
  }

  return ht_set_entry(t->entries, t->cap, key, v, &t->size);
}

size_t ht_size(ht *table) { return table->size; }

hti ht_iterator(ht *t) {
  hti it;
  it._table = t;
  it._index = 0;
  return it;
}

bool ht_next(hti *it) {
  ht *t = it->_table;
  while (it->_index < t->cap) {
    size_t i = it->_index++;
    if (t->entries[i].key != NULL) {
      hte e = t->entries[i];
      it->key = e.key;
      it->value = e.val;
      return true;
    }
  }

  return false;
}
