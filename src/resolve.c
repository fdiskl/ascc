// all functions related to resolving identifiers
// (part of parser)

#include "common.h"
#include "driver.h"
#include "parser.h"
#include "table.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// NOTE: unique names (int's) stored in hash table are not saved on heap but
// casted to ptr's. Be careful :)

int var_name_idx_counter = 0;

int get_name() { return ++var_name_idx_counter; }

static bool is_lvalue(expr *e) {
  return e->t == EXPR_VAR; // for now
}

void resolve_decl(parser *p, decl *d);

void enter_scope(parser *p) {
  // set as first in linked list
  ht *t = ht_create();
  ht_set_next_table(t, p->ident_ht_list_head);
  p->ident_ht_list_head = t;

  vec_push_back(tables_to_destroy, t);
}

void exit_scope(parser *p) {
  ht *tmp = p->ident_ht_list_head;
  p->ident_ht_list_head = ht_get_next_table(tmp);

  vec_pop_back(tables_to_destroy); // FIXME, will work for now, but isn't a good
                                   // idea, in future make so parser stores
                                   // indexes and free's by them

  ht_destroy(tmp);
}

void *find_entry(parser *p, string name) {
  void *entry;
  for (ht *curr = p->ident_ht_list_head; curr != NULL;
       curr = ht_get_next_table(curr)) {

    entry = ht_get(curr, (char *)name);
    if (entry != NULL)
      break;
  }

  return entry;
}

void resolve_expr(parser *p, expr *e) {
  switch (e->t) {
  case EXPR_INT_CONST:
    break;
  case EXPR_UNARY:
    if ((e->v.u.t == UNARY_POSTFIX_DEC || e->v.u.t == UNARY_POSTFIX_INC ||
         e->v.u.t == UNARY_PREFIX_DEC || e->v.u.t == UNARY_PREFIX_INC) &&
        (!is_lvalue(e->v.u.e))) {
      fprintf(stderr, "invalid lvalue (%d:%d-%d:%d)\n", e->pos.line_start,
              e->pos.pos_start, e->pos.line_end, e->pos.pos_end);
      after_error();
    }
    resolve_expr(p, e->v.u.e);
    break;
  case EXPR_BINARY:
    resolve_expr(p, e->v.b.l);
    resolve_expr(p, e->v.b.r);
    break;
  case EXPR_ASSIGNMENT:
    if (!is_lvalue(e->v.assignment.l)) {
      fprintf(stderr, "invalid lvalue (%d:%d-%d:%d)\n", e->pos.line_start,
              e->pos.pos_start, e->pos.line_end, e->pos.pos_end);
      after_error();
    }

    resolve_expr(p, e->v.assignment.l);
    resolve_expr(p, e->v.assignment.r);

    break;
  case EXPR_VAR: {
    void *entry = find_entry(p, e->v.var.name);
    if (entry == NULL) {
      fprintf(stderr, "undefined var %s (%d:%d-%d:%d)\n", e->v.var.name,
              e->pos.line_start, e->pos.pos_start, e->pos.line_end,
              e->pos.pos_end);
      after_error();
    } else {
      e->v.var.name_idx = (intptr_t)entry;
    }

    break;
  }
  case EXPR_TERNARY:
    resolve_expr(p, e->v.ternary.cond);
    resolve_expr(p, e->v.ternary.then);
    resolve_expr(p, e->v.ternary.elze);
    break;
  }
}

void resolve_decl(parser *p, decl *d) {
  if (d->t == DECL_FUNC)
    return; // skip for now

  void *e = ht_get(p->ident_ht_list_head, d->v.var.name);
  if (e != NULL) {
    fprintf(stderr, "duplicate variable declaration with name %s (%d:%d-%d:%d)",
            d->v.var.name, d->pos.line_start, d->pos.pos_start, d->pos.line_end,
            d->pos.pos_end);
    after_error();
  }
  int new_name = get_name();
  d->v.var.name_idx = new_name;

  const char *new_key = ht_set(p->ident_ht_list_head, d->v.var.name,
                               (void *)((intptr_t)new_name));
  assert(new_key);
}
