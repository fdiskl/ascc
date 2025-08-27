// all functions related to resolving identifiers
// (part of parser)

#include "arena.h"
#include "common.h"
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

void resolve_block_stmt(parser *p, stmt *s);

void resolve_decl(parser *p, decl *d);
static void resolve_stmt(parser *p, stmt *s) {}

void resolve_block_stmt(parser *p, stmt *s) {
  ++curr_scope;

  for (size_t i = 0; i < s->v.block.items_len; ++i) {
    block_item it = s->v.block.items[i];
    if (it.d != NULL)
      resolve_decl(p, it.d);
    else
      resolve_stmt(p, it.s);
  }

  TODO();

  --curr_scope;
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
    idente *entry = ht_get(p->identht, e->v.var.name);
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
  if (d->t == DECL_FUNC)
    return; // skip for now, TODO

  idente *e = ht_get(p->identht, d->v.var.name);
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

  const char *new_name = ht_set(p->identht, d->v.var.name, newe);
  assert(new_name);

  if (d->v.var.init != NULL)
    resolve_expr(p, d->v.var.init);
}
