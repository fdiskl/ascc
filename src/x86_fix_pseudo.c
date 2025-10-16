
#include "common.h"
#include "parser.h"
#include "strings.h"
#include "table.h"
#include "typecheck.h"
#include "x86.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

static int max_offset = 0;
static int offset = 0;
static ht *offset_table;

// defined in x86.c
x86_instr *alloc_x86_instr(x86_asm_gen *ag, int op);

// TODO: register allocation, graph coloring etc.
static void fix_pseudo_op(x86_op *op, ht *bst) {
  if (op->t != X86_OP_PSEUDO)
    return;

  be_syme *be = ht_get(bst, op->v.pseudo);
  assert(be != NULL && be->t == BE_SYME_OBJ);
  if (be->v.obj.is_static) {
    op->t = X86_OP_DATA;
    op->v.data = op->v.pseudo;
    return;
  }

  op->t = X86_OP_STACK;

  void *e = ht_get(offset_table, op->v.pseudo);
  if (e != NULL) {
    int d = (int)(intptr_t)e;
    if (d > max_offset)
      max_offset = d;
    op->v.stack_offset = d;
  } else {
    switch (be->v.obj.type) {
    case X86_LONGWORD:
      offset += 4;
      break;
    case X86_QUADWORD:
      offset += 8;
      offset = (offset + 7) & ~7; // align to 8
      break;
    case X86_BYTE:
      UNREACHABLE();
      break;
    }
    if (offset > max_offset)
      max_offset = offset;
    ht_set(offset_table, op->v.pseudo, (void *)(intptr_t)offset);

    op->v.stack_offset = offset;
  }

  if (op->v.stack_offset > max_offset)
    max_offset = op->v.stack_offset;
}

static void fix_pseudo_for_instr(x86_instr *i, ht *bst) {
  switch (i->op) {
  case X86_NOT:
  case X86_NEG:
  case X86_IDIV:
  case X86_INC:
  case X86_DEC:
  case X86_PUSH:
    fix_pseudo_op(&i->v.unary.src, bst);
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
  case X86_MOVSX:
    fix_pseudo_op(&i->v.binary.src, bst);
    fix_pseudo_op(&i->v.binary.dst, bst);
    break;
  case X86_SETCC:
    fix_pseudo_op(&i->v.setcc.op, bst);
    break;
  case X86_RET:
  case X86_CDQ:
  case X86_LABEL:
  case X86_JMP:
  case X86_JMPCC:
  case X86_COMMENT:
  case X86_CALL:
    break;
  }
}

#ifdef PRINT_VARS_LAYOUT_X86
typedef struct _tmp tmp_entry;
struct _tmp {
  int offset;
  const char *name;
};

int tmp_entry_cmp(const void *a, const void *b) {
  tmp_entry *aa = (tmp_entry *)a;
  tmp_entry *bb = (tmp_entry *)b;

  return aa->offset < bb->offset;
}

#endif

int fix_pseudo_for_func(x86_asm_gen *ag, x86_func *f, ht *bst) {
  max_offset = 0;
  offset = 0;

  offset_table = ht_create();

  for (x86_instr *i = f->first; i != NULL; i = i->next)
    fix_pseudo_for_instr(i, bst);

#ifdef PRINT_VARS_LAYOUT_X86
  x86_instr *head = alloc_x86_instr(ag, X86_COMMENT);
  head->v.comment = new_string("---- vars layout ----");
  head->prev = NULL;
  x86_instr *tail = head;

  // get all entries from offset table
  hti it = ht_iterator(offset_table);
  VEC(tmp_entry) arr;
  vec_init(arr);
  while (ht_next(&it)) {
    tmp_entry val;
    val.name = it.key;
    val.offset = (int)(intptr_t)it.value;
    vec_push_back(arr, val);
  }

  // sort entries
  qsort(arr.data, arr.size, sizeof(tmp_entry), tmp_entry_cmp);

  // print entries
  vec_foreach(tmp_entry, arr, it) {
    x86_instr *c = alloc_x86_instr(ag, X86_COMMENT);
    c->v.comment = string_sprintf(" %s: -%d(%%rbp)", it->name, it->offset);
    c->prev = tail;
    tail->next = c;
    tail = c;
  }

  // print static vars
  it = ht_iterator(bst);
  while (ht_next(&it)) {
    be_syme *e = it.value;
    if (e->t != BE_SYME_FN && !e->v.obj.is_static)
      continue;
    x86_instr *c = alloc_x86_instr(ag, X86_COMMENT);
    c->v.comment = string_sprintf(" %s: %s(%%rsp)", it.key, it.key);
    c->prev = tail;
    tail->next = c;
    tail = c;
  }

  x86_instr *c = alloc_x86_instr(ag, X86_COMMENT);
  c->v.comment = new_string("--------------------");
  c->prev = tail;
  tail->next = c;
  tail = c;

  f->first->prev = tail;
  tail->next = f->first;
  f->first = head;

#endif

  ht_destroy(offset_table);

  return (max_offset + 15) & ~15; // round to 16
}
