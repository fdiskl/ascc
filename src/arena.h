#ifndef _ASCC_ARENA_H
#define _ASCC_ARENA_H

#include <stdalign.h>
#include <stddef.h>

typedef struct _arena arena;
typedef struct _arena_chunk _arena_chunk;

// Single chunk in the arena
struct _arena_chunk {
  char *region;       // pointer to allocated memory block
  size_t size;        // total size of the block in bytes
  size_t index;       // current allocation offset in the block
  _arena_chunk *next; // pointer to next chunk
};

// Arena structure managing multiple chunks
struct _arena {
  _arena_chunk *head; // head chunk of the linked list
  _arena_chunk *curr; // current chunk used for allocations
  size_t chunkSize;   // preferred chunk size for new allocations
};

// Initializes an arena with default chunk size.
void init_arena(arena *a);

// Initializes an arena with default chunk size.
// Returns pointer to new arena or NULL on failure.
arena *new_arena();

// Expands the arena by adding a new chunk with at least newSize bytes.
void expand_arena(arena *a, size_t newSize);

// Resets arena allocations to zero but keeps allocated memory intact.
void clear_arena(arena *a);

// Frees all memory used by the arena. (When using init_arena)
void free_arena(arena *a);

// Fress all memory used by the arena and frees arena ptr itself (when using
// new_arena)
void destroy_arena(arena *a);

// Copies contents from src arena to dst arena.
// Returns number of bytes copied.
size_t copy_arena(arena *dst, const arena *src);

// Allocates memory of sizeOfElement bytes aligned to alignmentOfElement from
// arena. Returns pointer to allocated memory or NULL if allocation fails.
void *arena_alloc(arena *a, size_t alignmentOfElement, size_t sizeOfElement);

// Convenience macros to allocate objects or arrays from arena with correct
// alignment.
#define ARENA_ALLOC_OBJ(arena_ptr, Type)                                       \
  (Type *)arena_alloc((arena_ptr), alignof(Type), sizeof(Type))

#define ARENA_ALLOC_ARRAY(arena_ptr, Type, Count)                              \
  (Type *)arena_alloc((arena_ptr), alignof(Type), sizeof(Type) * (Count))

#endif
