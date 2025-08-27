// all functions related to resolving identifiers
// (part of parser)

#include "arena.h"
#include "common.h"
#include "driver.h"
#include "parser.h"
#include "table.h"
#include "vec.h"
#include <assert.h>
#include <stdio.h>

static int curr_scope = 0;

idente *alloc_new_entry(parser *p) {
  static int counter = 0;
  idente *newe = ARENA_ALLOC_OBJ(&p->idente_arena, idente);
  newe->scope = curr_scope;
  newe->nameidx = ++counter;

  return newe;
}

static bool is_lvalue(expr *e) {
  return e->t == EXPR_VAR; // for now
}

void resolve_decl(parser *p, decl *d);

void enter_scope(parser *p) {
  ++curr_scope;

  // set as first in linked list
  ht *t = ht_create();
  ht_set_next_table(t, p->ident_ht_list_head);
  p->ident_ht_list_head = t;

  vec_push_back(tables_to_destroy, t);
}

void exit_scope(parser *p) {
  --curr_scope;

  ht *tmp = p->ident_ht_list_head;
  p->ident_ht_list_head = ht_get_next_table(tmp);

  vec_pop_back(tables_to_destroy); // FIXME, will work for now, but isn't a good
                                   // idea, in future make so parser stores
                                   // indexes and free's by them

  ht_destroy(tmp);
}

void resolve_expr(parser *p, expr *e) {
  switch (e->t) {
  case EXPR_INT_CONST:
    break;
  case EXPR_UNARY:
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
    idente *entry = ht_get(p->ident_ht_list_head, e->v.var.name);
    if (entry == NULL) {
      fprintf(stderr, "undefined var %s (%d:%d-%d:%d)\n", e->v.var.name,
              e->pos.line_start, e->pos.pos_start, e->pos.line_end,
              e->pos.pos_end);
      after_error();
    } else {
      e->v.var.name_idx = entry->nameidx;
    }

    break;
  }
  }
}

void resolve_decl(parser *p, decl *d) {
  idente *e = ht_get(p->ident_ht_list_head, d->v.var.name);
  if (e != NULL) {
    if (e->scope == curr_scope) {
      fprintf(stderr,
              "duplicate variable declaration with name %s (%d:%d-%d:%d)",
              d->v.var.name, d->pos.line_start, d->pos.pos_start,
              d->pos.line_end, d->pos.pos_end);
      after_error();
    }
  }
  idente *newe = alloc_new_entry(p);
  d->v.var.name_idx = newe->nameidx;

  const char *new_name = ht_set(p->ident_ht_list_head, d->v.var.name, newe);
  assert(new_name);

  if (d->v.var.init != NULL)
    resolve_expr(p, d->v.var.init);
}
