
#include "table.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

typedef struct _hte hte;

struct _hte {
  union {
    const char *key;
    int idx;
  } v;
  void *val;
};

#define NULL_INT_KEY -1

struct ht {
  hte *entries;
  size_t cap;
  size_t size;
  int is_keys_strings;
  ht *next;
};

ht *ht_create(void) {
  ht *t = (ht *)malloc(sizeof(ht));

  assert(t);
  t->size = 0;
  t->cap = HT_INITIAL_CAPACITY;

  t->next = NULL;
  t->is_keys_strings = true;

  t->entries = calloc(t->cap, sizeof(hte));
  assert(t->entries);

  return t;
}

ht *ht_create_int(void) {
  ht *t = (ht *)malloc(sizeof(ht));

  assert(t);
  t->size = 0;
  t->cap = HT_INITIAL_CAPACITY;

  t->next = NULL;
  t->is_keys_strings = false;

  t->entries = calloc(t->cap, sizeof(hte));
  assert(t->entries);

  for (size_t i = 0; i < t->cap; ++i) {
    t->entries[i].v.idx = NULL_INT_KEY;
  }

  return t;
}

ht *ht_get_next_table(ht *t) { return t->next; }
void ht_set_next_table(ht *t, ht *next) { t->next = next; }

void ht_destroy(ht *t) {
  if (t->is_keys_strings)
    for (size_t i = 0; i < t->cap; ++i)
      free((void *)t->entries[i].v.key);

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
  while (t->entries[idx].v.key != NULL) {
    if (strcmp(key, t->entries[idx].v.key) == 0)
      return t->entries[idx].val;
    ++idx;
    if (idx >= t->cap)
      idx = 0; // wrap around
  }

  return NULL;
}

void *ht_get_int(ht *t, int key) {
  size_t idx = (size_t)(key & (t->cap - 1));

  while (t->entries[idx].v.idx != NULL_INT_KEY) {
    if (t->entries[idx].v.idx == key)
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

  while (entries[idx].v.key != NULL) {
    if (strcmp(key, entries[idx].v.key) == 0) {
      entries[idx].val = v;
      return entries[idx].v.key;
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

  entries[idx].v.key = key;
  entries[idx].val = v;
  return key;
}

static bool ht_set_entry_int(hte *entries, size_t cap, int key, void *v,
                             size_t *psize) {
  size_t idx = (size_t)(key & (cap - 1));

  while (entries[idx].v.idx != NULL_INT_KEY) {
    if (entries[idx].v.idx == key) {
      entries[idx].val = v; // update existing key
      return true;
    }

    ++idx;
    if (idx >= cap)
      idx = 0; // wrap around
  }

  // key not found, insert
  entries[idx].v.idx = key;
  entries[idx].val = v;
  if (psize != NULL)
    ++(*psize);
  return true;
}

// twice size, true on success, false on failure
static bool ht_expand(ht *t) {
  size_t newcap = t->cap * 2;
  if (newcap < t->cap)
    return false;

  hte *new_entries = calloc(newcap, sizeof(hte));
  assert(new_entries);

  if (!t->is_keys_strings) {
    for (size_t i = 0; i < newcap; ++i)
      new_entries[i].v.idx = NULL_INT_KEY;
  }

  for (size_t i = 0; i < t->cap; ++i) {
    hte e = t->entries[i];
    if (t->is_keys_strings) {
      if (e.v.key != NULL)
        ht_set_entry(new_entries, newcap, e.v.key, e.val, NULL);
    } else {
      if (e.v.idx != NULL_INT_KEY)
        ht_set_entry_int(new_entries, newcap, e.v.idx, e.val, NULL);
    }
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

bool ht_set_int(ht *t, int key, void *v) {
  assert(v != NULL);

  if (t->size >= t->cap / 2) {
    if (!ht_expand(t))
      return false;
  }

  return ht_set_entry_int(t->entries, t->cap, key, v, &t->size);
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

    if (t->is_keys_strings) {
      if (t->entries[i].v.key != NULL) {
        hte e = t->entries[i];
        it->key = e.v.key;
        it->idx = NULL_INT_KEY;

        it->value = e.val;
        return true;
      }
    } else {
      if (t->entries[i].v.idx != NULL_INT_KEY) {
        hte e = t->entries[i];
        it->key = NULL;
        it->idx = e.v.idx;

        it->value = e.val;
        return true;
      }
    }
  }

  return false;
}
