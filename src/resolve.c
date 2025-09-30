// all functions related to resolving identifiers
// (part of parser)

#include "arena.h"
#include "common.h"
#include "driver.h"
#include "parser.h"
#include "strings.h"
#include "table.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

extern arena ptr_arena; // (main.c)

// labels_ht stores label idx casted as void*
// gotos_to_check_ht stores pointer to goto node

int var_name_idx_counter = 0;
int label_idx_counter = 0;

static int scope = 0;

static int get_label() { return ++label_idx_counter; }
static int get_name() { return ++var_name_idx_counter; }

static ident_entry *alloc_symt_entry(parser *p, string original_name,
                                     char linkage, string name) {
  ident_entry *e = ARENA_ALLOC_OBJ(&p->ident_entry_arena, ident_entry);
  e->has_linkage = linkage;
  e->original_name = original_name;
  e->name = name;
  e->scope = scope;

  return e;
}

static ident_entry *create_symt_entry(parser *p, string original_name,
                                      char linkage, int name_idx) {
  return alloc_symt_entry(p, original_name, linkage,
                          string_sprintf("%s_%d", original_name, name_idx));
}

static ident_entry *new_symt_entry(parser *p, string name, char linkage) {
  return create_symt_entry(p, name, linkage, get_name());
}

static bool is_lvalue(expr *e) {
  return e->t == EXPR_VAR; // for now
}

bool is_constant_expr(expr *e) {
  return e->t == EXPR_INT_CONST; // TODO
}

void check_for_constant_expr(expr *e) {
  if (!is_constant_expr(e)) {
    fprintf(stderr, "expected constant expr (%d:%d-%d:%d)\n", e->pos.line_start,
            e->pos.pos_start, e->pos.line_end, e->pos.pos_end);
    exit(1);
  } // TODO: add more const exprs
}

string hash_for_constant_expr(expr *e) {
  // anything if it will be unique
  check_for_constant_expr(e);
  return string_sprintf("%d", e->v.intc); // for now only intc
}

void resolve_label_stmt(parser *p, stmt *s) {
  if (ht_get(p->labels_ht, s->v.label.label) != NULL) {
    fprintf(stderr, "duplicate label %s (%d:%d-%d:%d)\n", s->v.label.label,
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);
    exit(1);
  }

  ht_set(p->labels_ht, s->v.label.label,
         (void *)((intptr_t)(s->v.label.label_idx = get_label())));
}

void resolve_goto_stmt(parser *p, stmt *s) {
  ht_set(p->gotos_to_check_ht, s->v.goto_stmt.label, (void *)s);
}

void resolve_decl(parser *p, decl *d);

void enter_scope(parser *p) {
  ++scope;
  // set as first in linked list
  ht *t = ht_create();
  ht_set_next_table(t, p->ident_ht_list_head);
  p->ident_ht_list_head = t;
}

void exit_scope(parser *p) {
  --scope;
  ht *tmp = p->ident_ht_list_head;
  p->ident_ht_list_head = ht_get_next_table(tmp);

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

void resolve_func_call_expr(parser *p, expr *e) {
  ident_entry *entry = find_entry(p, e->v.func_call.name);
  if (entry == NULL) {
    fprintf(stderr, "call to undeclared func %s (%d:%d-%d:%d)\n",
            e->v.func_call.name, e->pos.line_start, e->pos.pos_start,
            e->pos.line_end, e->pos.pos_end);
    exit(1);
  }

  e->v.func_call.name = entry->name;
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
      exit(1);
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
      exit(1);
    }

    resolve_expr(p, e->v.assignment.l);
    resolve_expr(p, e->v.assignment.r);

    break;
  case EXPR_VAR: {
    ident_entry *entry = find_entry(p, e->v.var.original_name);
    if (entry == NULL) {
      fprintf(stderr, "undefined var %s (%d:%d-%d:%d)\n",
              e->v.var.original_name, e->pos.line_start, e->pos.pos_start,
              e->pos.line_end, e->pos.pos_end);
      exit(1);
    } else {
      e->v.var.name = entry->name;
    }

    break;
  }
  case EXPR_TERNARY:
    resolve_expr(p, e->v.ternary.cond);
    resolve_expr(p, e->v.ternary.then);
    resolve_expr(p, e->v.ternary.elze);
    break;
  case EXPR_FUNC_CALL:
    resolve_func_call_expr(p, e);
    break;
  }
}

// try to find last switch in stack_loop_resolve_info, -1 if not found, idx
// otherwise
static int find_last_switch(parser *p) {
  for (int i = p->stack_loop_resolve_info.size - 1; i >= 0; --i) {
    if (p->stack_loop_resolve_info.data[i].t == LOOP_RESOLVE_SWITCH)
      return i;
  }

  return -1;
}

// try to find last loop in stack_loop_resolve_info, -1 if not found, idx
// otherwise
static int find_last_loop(parser *p) {
  for (int i = p->stack_loop_resolve_info.size - 1; i >= 0; --i) {
    if (p->stack_loop_resolve_info.data[i].t == LOOP_RESOLVE_LOOP)
      return i;
  }

  return -1;
}

void resolve_break_stmt(parser *p, stmt *s) {
  if (p->stack_loop_resolve_info.size <= 0) {
    fprintf(stderr, "can't use break outside of loop or switch (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i =
      &p->stack_loop_resolve_info.data[p->stack_loop_resolve_info.size - 1];

  switch (i->t) {
  case LOOP_RESOLVE_LOOP:
    s->v.break_stmt.idx = i->v.l.break_idx;
    break;
  case LOOP_RESOLVE_SWITCH:
    s->v.break_stmt.idx = i->v.s.break_idx;
    break;
  }
}

void resolve_continue_stmt(parser *p, stmt *s) {

  int loop_idx = find_last_loop(p);

  if (p->stack_loop_resolve_info.size <= 0 || loop_idx == -1) {
    fprintf(stderr, "can't use continue outside of loop (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i = &p->stack_loop_resolve_info.data[loop_idx];

  s->v.continue_stmt.idx = i->v.l.continue_idx;
}

void resolve_case_stmt(parser *p, stmt *s) {

  int switch_idx = find_last_switch(p);

  if (p->stack_loop_resolve_info.size <= 0 || switch_idx == -1) {
    fprintf(stderr, "can't use case outside of switch (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i = &p->stack_loop_resolve_info.data[switch_idx];

  string key = hash_for_constant_expr(s->v.case_stmt.e);
  stmt *old = (stmt *)ht_get(i->v.s.cases, key);
  if (old != NULL) {
    fprintf(
        stderr,
        "case with such expr already defined at %d:%d-%d:%d (%d:%d-%d:%d)\n",
        old->pos.line_start, old->pos.pos_start, old->pos.line_end,
        old->pos.pos_end, s->pos.line_start, s->pos.pos_start, s->pos.line_end,
        s->pos.pos_end);

    exit(1);
  }

  ht_set(i->v.s.cases, key, (void *)s);

  s->v.case_stmt.label_idx = get_label();
}

void resolve_default_stmt(parser *p, stmt *s) {
  int switch_idx = find_last_switch(p);
  if (p->stack_loop_resolve_info.size <= 0 || switch_idx == -1) {
    fprintf(stderr, "can't use default outside of switch (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i = &p->stack_loop_resolve_info.data[switch_idx];

  if (i->v.s.default_stmt != NULL) {
    stmt *old = i->v.s.default_stmt;
    fprintf(stderr, "default already defined at %d:%d-%d:%d (%d:%d-%d:%d)\n",
            old->pos.line_start, old->pos.pos_start, old->pos.line_end,
            old->pos.pos_end, s->pos.line_start, s->pos.pos_start,
            s->pos.line_end, s->pos.pos_end);

    exit(1);
  }

  i->v.s.default_stmt = s;
  s->v.default_stmt.label_idx = get_label();
}

void enter_loop(parser *p, stmt *s) {
  loop_resolve_info i;

  i.t = LOOP_RESOLVE_LOOP;
  i.s = s;
  int breakl = i.v.l.break_idx = get_label();
  int continuel = i.v.l.continue_idx = get_label();

  switch (s->t) {
  case STMT_WHILE:
    s->v.while_stmt.break_label_idx = breakl;
    s->v.while_stmt.continue_label_idx = continuel;
    break;
  case STMT_DOWHILE:
    s->v.dowhile_stmt.break_label_idx = breakl;
    s->v.dowhile_stmt.continue_label_idx = continuel;
    break;
  case STMT_FOR:
    s->v.for_stmt.break_label_idx = breakl;
    s->v.for_stmt.continue_label_idx = continuel;
    break;
  default:
    UNREACHABLE();
  }

  vec_push_back(p->stack_loop_resolve_info, i);
}

void exit_loop(parser *p, stmt *s) {
  assert(
      p->stack_loop_resolve_info.size >= 0 &&
      p->stack_loop_resolve_info.data[p->stack_loop_resolve_info.size - 1].s ==
          s);

  vec_pop_back(p->stack_loop_resolve_info);
}

void enter_switch(parser *p, stmt *s) {
  loop_resolve_info i;

  i.t = LOOP_RESOLVE_SWITCH;
  i.s = s;
  i.v.s.default_stmt = NULL;
  s->v.switch_stmt.break_label_idx = i.v.s.break_idx = get_label();
  i.v.s.cases = ht_create();

  vec_push_back(p->stack_loop_resolve_info, i);
}

void exit_switch(parser *p, stmt *s) {
  assert(
      p->stack_loop_resolve_info.size >= 0 &&
      p->stack_loop_resolve_info.data[p->stack_loop_resolve_info.size - 1].s ==
          s);
  loop_resolve_info *i =
      &p->stack_loop_resolve_info.data[p->stack_loop_resolve_info.size - 1];

  s->v.switch_stmt.default_stmt = i->v.s.default_stmt;

  hti it = ht_iterator(i->v.s.cases);
  int c = 0;
  while (ht_next(&it)) {
    ++c;
  }

  stmt **arr = ARENA_ALLOC_ARRAY(&ptr_arena, stmt *, c);

  it = ht_iterator(i->v.s.cases);
  int j = 0;
  while (ht_next(&it)) {
    arr[j++] = (stmt *)it.value;
  }

  s->v.switch_stmt.cases = arr;
  s->v.switch_stmt.cases_len = c;

  ht_destroy(i->v.s.cases);
  vec_pop_back(p->stack_loop_resolve_info);
}

void enter_func(parser *p, decl *f) {
  f->scope = scope;
  ident_entry *e =
      ht_get(p->ident_ht_list_head, f->v.func.name); // check only curr scope
  if (e != NULL && !e->has_linkage) {
    fprintf(stderr, "function with name %s already defined (%d:%d-%d:%d)\n",
            f->v.func.name, f->pos.line_start, f->pos.pos_start,
            f->pos.line_end, f->pos.pos_end);

    exit(1);
  }

  if (e == NULL) {
    e = alloc_symt_entry(p, f->v.func.name, true, f->v.func.name);

    ht_set(p->ident_ht_list_head, f->v.func.name, e);
  }
  f->v.func.name = f->v.func.name;

  enter_scope(p);
}

void enter_body_of_func(parser *p, decl *f) {
  p->labels_ht = ht_create();
  p->gotos_to_check_ht = ht_create();
  vec_init(p->stack_loop_resolve_info);
}

void exit_func(parser *p, decl *f) { exit_scope(p); }

void exit_func_with_body(parser *p, decl *f) {
  hti it = ht_iterator(p->gotos_to_check_ht);
  while (ht_next(&it)) {
    void *e = ht_get(p->labels_ht, it.key);
    if (e == NULL) {
      fprintf(stderr, "goto to undeclared label '%s'\n",
              it.key); // would be nice to add location info, but i am too lazy.
                       // TODO ig
      exit(1);
    }

    stmt *s = (stmt *)it.value;
    assert(s->t == STMT_GOTO);
    s->v.goto_stmt.label_idx = ((int)((intptr_t)e));
  }

  exit_scope(p);

  vec_free(p->stack_loop_resolve_info);
  ht_destroy(p->labels_ht);
  ht_destroy(p->gotos_to_check_ht);
}

ident_entry *resolve_filescope_var_decl(parser *p, string name, ast_pos pos) {
  ident_entry *new_e = alloc_symt_entry(p, name, true, name);

  const char *new_key = ht_set(p->ident_ht_list_head, name, new_e);
  assert(new_key);

  return new_e;
}

ident_entry *resolve_param(parser *p, string name, ast_pos pos) {
  ident_entry *e = ht_get(p->ident_ht_list_head, name); // check only curr scope
  if (e != NULL) {
    fprintf(stderr, "duplicate param declaration with name %s (%d:%d-%d:%d)",
            name, pos.line_start, pos.pos_start, pos.line_end, pos.pos_end);
    exit(1);
  }
  ident_entry *new_e = new_symt_entry(p, name, false);

  const char *new_key = ht_set(p->ident_ht_list_head, name, new_e);
  assert(new_key);

  return new_e;
}

ident_entry *resolve_local_var_decl(parser *p, string name, ast_pos pos,
                                    sct sc) {
  ident_entry *e = ht_get(p->ident_ht_list_head, name); // check only curr scope
  if (e != NULL && !(e->has_linkage && sc == SC_EXTERN)) {
    fprintf(stderr,
            "conflicting local declarations with name %s (%d:%d-%d:%d)\n", name,
            pos.line_start, pos.pos_start, pos.line_end, pos.pos_end);
    exit(1);
  }

  if (sc == SC_EXTERN) {
    ident_entry *new_e = alloc_symt_entry(p, name, true, name);
    const char *new_key = ht_set(p->ident_ht_list_head, name, new_e);
    assert(new_key);

    return new_e;
  } else {
    ident_entry *new_e = new_symt_entry(p, name, false);

    const char *new_key = ht_set(p->ident_ht_list_head, name, new_e);
    assert(new_key);

    return new_e;
  }
}

// set param to true when resolving param
// returns id of new ident
ident_entry *resolve_var_decl(parser *p, string name, ast_pos pos, char param,
                              sct sc) {
  if (scope == 0)
    return resolve_filescope_var_decl(p, name, pos);
  if (param)
    return resolve_param(p, name, pos);
  else
    return resolve_local_var_decl(p, name, pos, sc);
}
