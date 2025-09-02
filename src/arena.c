#include "arena.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static void *os_alloc(size_t size) {
  return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
static void os_free(void *ptr, size_t size) {
  VirtualFree(ptr, 0, MEM_RELEASE);
}
static size_t os_page_size() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
}
#else
#include <sys/mman.h>
#include <unistd.h>
static void *os_alloc(size_t size) {
  void *p =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  return p == MAP_FAILED ? NULL : p;
}
static void os_free(void *ptr, size_t size) { munmap(ptr, size); }
static size_t os_page_size() { return sysconf(_SC_PAGESIZE); }
#endif

static _arena_chunk *alloc_chunk(size_t size) {
  _arena_chunk *chunk = malloc(sizeof(_arena_chunk));
  assert(chunk != NULL);

  chunk->region = os_alloc(size);
  assert(chunk->region != MAP_FAILED);

  chunk->size = size;
  chunk->index = 0;
  chunk->next = NULL;

  return chunk;
}

void init_arena(arena *a, size_t alignment_of_element, size_t size_of_element) {
  size_t size = os_page_size();
  if (size == 0)
    return;

  _arena_chunk *chunk = alloc_chunk(size);
  a->head = chunk;
  a->curr = chunk;
  a->chunkSize = size;
  a->el_size = size_of_element;
  a->el_alignment = alignment_of_element;
}

arena *new_arena(size_t alignment_of_element, size_t size_of_element) {
  arena *a = malloc(sizeof(arena));
  assert(a != NULL);

  init_arena(a, alignment_of_element, size_of_element);

  return a;
}

void clear_arena(arena *a) {
  if (!a)
    return;

  for (_arena_chunk *chunk = a->head; chunk; chunk = chunk->next) {
    chunk->index = 0;
  }

  a->curr = a->head;
}

void destroy_arena(arena *a) {
  free_arena(a);
  free(a);
}

void free_arena(arena *a) {
  if (!a)
    return;

  _arena_chunk *chunk = a->head;
  while (chunk) {
    _arena_chunk *next = chunk->next;
    os_free(chunk->region, chunk->size); // free mmap region
    free(chunk);
    chunk = next;
  }
}

size_t copy_arena(arena *dst, const arena *src) {
  if (!dst || !src)
    return 0;

  dst->el_size = src->el_size;
  dst->el_alignment = src->el_alignment;

  _arena_chunk *srcChunk = src->head;
  _arena_chunk **dstLink = &dst->head;
  size_t totalCopied = 0;

  while (srcChunk) {
    _arena_chunk *dstChunk = *dstLink;
    if (!dstChunk) {
      dstChunk = alloc_chunk(dst->chunkSize);
      *dstLink = dstChunk;
    }

    size_t toCopy = srcChunk->index;
    if (toCopy > dstChunk->size)
      toCopy = dstChunk->size;

    memcpy(dstChunk->region, srcChunk->region, toCopy);
    dstChunk->index = toCopy;
    totalCopied += toCopy;

    srcChunk = srcChunk->next;
    dstLink = &dstChunk->next;
  }

  dst->curr = dst->head;
  while (dst->curr->next && dst->curr->next->index > 0)
    dst->curr = dst->curr->next;

  return totalCopied;
}

void *_arena_alloc(arena *a, size_t size, size_t alignment);

void *arena_alloc_arr(arena *a, size_t n) {
  size_t alignment = a->el_alignment;
  size_t size = a->el_size;
  return _arena_alloc(a, size * n, alignment);
}

void *arena_alloc(arena *a) {
  size_t alignment = a->el_alignment;
  size_t size = a->el_size;
  return _arena_alloc(a, size, alignment);
}

void *_arena_alloc(arena *a, size_t size, size_t alignment) {

  if (!a || size == 0)
    return NULL;

  if (alignment == 0)
    alignment = 1;
  if ((alignment & (alignment - 1)) != 0) {
    fprintf(stderr, "arena_alloc error: alignment %zu is not power-of-two\n",
            alignment);
    return NULL;
  }

  if (size > a->chunkSize) {
    fprintf(stderr, "arena_alloc error: allocation size %zu > chunk size %zu\n",
            size, a->chunkSize);
    return NULL;
  }

  _arena_chunk *chunk = a->curr;

  while (chunk) {
    uintptr_t base = (uintptr_t)((char *)chunk->region + chunk->index);
    size_t misalign = base & (alignment - 1);
    size_t offset = misalign ? alignment - misalign : 0;

    if (chunk->index + offset + size <= chunk->size) {
      void *ptr = (char *)chunk->region + chunk->index + offset;
      chunk->index += offset + size;
      a->curr = chunk;
      return ptr;
    }

    if (!chunk->next)
      chunk->next = alloc_chunk(a->chunkSize);

    chunk = chunk->next;
  }

  return NULL;
}
