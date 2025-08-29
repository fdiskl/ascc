
#include "strings.h"
#include "table.h"
#include "x86.h"
#include <stdint.h>
#include <stdio.h>

static int max_offset = 0;

// defined in x86.c
x86_instr *alloc_x86_instr(x86_asm_gen *ag, int op);

#ifdef PRINT_VARS_LAYOUT_X86
ht *names_table;

typedef struct {
  const char *key;
  void *value;
} kv_pair;

static int cmp_varnames(const void *a, const void *b) {
  const kv_pair *ka = a;
  const kv_pair *kb = b;

  // skip leading 'v' and convert rest to int
  // assumes form of 'v%d'
  int ia = atoi(ka->key + 1);
  int ib = atoi(kb->key + 1);

  return ia - ib;
}
#endif

// TODO: register allocation, graph coloring etc.
static void fix_pseudo_op(x86_op *op) {
  if (op->t != X86_OP_PSEUDO)
    return;

#ifdef PRINT_VARS_LAYOUT_X86
  int pseudo_idx = op->v.pseudo_idx;
#endif

  op->t = X86_OP_STACK;
  op->v.stack_offset =
      (op->v.pseudo_idx) * 4; // will do for now, hash table later mb

#ifdef PRINT_VARS_LAYOUT_X86
  char buf[128];
  snprintf(buf, sizeof(buf), "v%d", pseudo_idx);
  ht_set(names_table, buf, (void *)((intptr_t)op->v.stack_offset));
#endif

  if (op->v.stack_offset > max_offset)
    max_offset = op->v.stack_offset;
}

static void fix_pseudo_for_instr(x86_instr *i) {
  switch (i->op) {
  case X86_NOT:
  case X86_NEG:
  case X86_IDIV:
    fix_pseudo_op(&i->v.unary.src);
    break;
  case X86_MOV:
  case X86_ADD:
  case X86_SUB:
  case X86_MULT:
  case X86_AND:
  case X86_OR:
  case X86_XOR:
  case X86_SHL:
  case X86_SAR:
  case X86_CMP:
    fix_pseudo_op(&i->v.binary.src);
    fix_pseudo_op(&i->v.binary.dst);
    break;
  case X86_SETCC:
    fix_pseudo_op(&i->v.setcc.op);
    break;
  case X86_RET:
  case X86_ALLOC_STACK:
  case X86_CDQ:
  case X86_LABEL:
  case X86_JMP:
  case X86_JMPCC:
  case X86_COMMENT:
    break;
  }
}

int fix_pseudo_for_func(x86_asm_gen *ag, x86_func *f) {
#ifdef PRINT_VARS_LAYOUT_X86
  names_table = ht_create();
#endif

  for (x86_instr *i = f->first; i != NULL; i = i->next)
    fix_pseudo_for_instr(i);

#ifdef PRINT_VARS_LAYOUT_X86

  hti it = ht_iterator(names_table);
  int count = 0;
  while (ht_next(&it)) {
    ++count;
    // x86_instr *i = alloc_x86_instr(ag, X86_COMMENT);
    // i->v.comment =
    //     string_sprintf("%s: -%d(%%rbp)", it.key, (int)((intptr_t)it.value));

    // i->next = NULL;
    // i->prev = NULL;

    // if (head == NULL) {
    //   head = i;
    //   tail = i;
    // } else {
    //   i->prev = tail;
    //   tail->next = i;
    //   tail = i;
    // }
  }

  kv_pair *arr = malloc(count * sizeof(kv_pair));

  size_t idx = 0;
  it = ht_iterator(names_table); // start new iteration
  while (ht_next(&it)) {
    arr[idx].key = it.key;
    arr[idx].value = it.value;
    idx++;
  }

  qsort(arr, count, sizeof(kv_pair), cmp_varnames);

  x86_instr *line1 = alloc_x86_instr(ag, X86_COMMENT);
  x86_instr *line2 = alloc_x86_instr(ag, X86_COMMENT);

  line1->v.comment = string_sprintf("\n\t#--- local var locations ---");
  line2->v.comment = string_sprintf("---------------------------\n\t#");

  line1->prev = NULL;
  line1->next = NULL;
  line2->prev = NULL;
  line2->next = NULL;

  x86_instr *head = line1;
  x86_instr *tail = line1;

  for (size_t j = 0; j < count; j++) {
    x86_instr *i = alloc_x86_instr(ag, X86_COMMENT);
    i->v.comment = string_sprintf("%s: -%d(%%rbp)", arr[j].key,
                                  (int)((intptr_t)arr[j].value));

    i->next = NULL;
    i->prev = tail;
    tail->next = i;
    tail = i;
  }

  free(arr);

  line2->prev = tail;
  tail->next = line2;
  tail = line2;

  if (f->first != NULL) {
    tail->next = f->first;
    f->first->prev = tail;
  }
  f->first = head;

  ht_destroy(names_table);
#endif

  return max_offset;
}
