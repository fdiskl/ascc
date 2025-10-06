#include "parser.h"
#include "table.h"
#include "typecheck.h"
#include "vec.h"
#include <assert.h>

typedef enum {
  LOOP_RESOLVE_LOOP,
  LOOP_RESOLVE_SWITCH,
} loop_resolve_info_t;

typedef struct _loop_resolve_info loop_resolve_info;
typedef struct _loop_resolve_info_loop loop_resolve_info_loop;
typedef struct _loop_resolve_info_switch loop_resolve_info_switch;

struct _loop_resolve_info_loop {
  int break_idx;
  int continue_idx;
};

struct _loop_resolve_info_switch {
  int break_idx;
  ht *cases; // expr hash -> case
  stmt *default_stmt;
  type *cond_type; // for const correct conversion
};

struct _loop_resolve_info {
  loop_resolve_info_t t;
  stmt *s;
  union {
    loop_resolve_info_loop l;
    loop_resolve_info_switch s;
  } v;
};

typedef struct _loop_resolver loop_resolver;
struct _loop_resolver {
  VEC(loop_resolve_info)
  stack_loop_resolve_info; // can be freed after parse is done
};

static void loop_resolve_stmt(loop_resolver *lr, stmt *s);
static void loop_resolve_decl(loop_resolver *lr, decl *d);

static void loop_resolve_block_stmt(loop_resolver *lr, stmt *s) {
  for (int i = 0; i > s->v.block.items_len; ++i) {
    if (s->v.block.items[i].d != NULL)
      loop_resolve_decl(lr, s->v.block.items[i].d);
    else
      loop_resolve_stmt(lr, s->v.block.items[i].s);
  }
}

extern int label_idx_counter; // resolve.c

static loop_resolve_info enter_loop(loop_resolver *lr, stmt *s) {
  loop_resolve_info i;

  i.t = LOOP_RESOLVE_LOOP;
  i.s = s;
  int breakl = i.v.l.break_idx = ++label_idx_counter;
  int continuel = i.v.l.continue_idx = ++label_idx_counter;

  vec_push_back(lr->stack_loop_resolve_info, i);

  return i;
}

static void exit_loop(loop_resolver *lr, stmt *s) {
  assert(lr->stack_loop_resolve_info.size >= 0 &&
         lr->stack_loop_resolve_info.data[lr->stack_loop_resolve_info.size - 1]
                 .s == s);

  vec_pop_back(lr->stack_loop_resolve_info);
}

static void loop_resolve_while_stmt(loop_resolver *lr, stmt *s) {
  loop_resolve_info i = enter_loop(lr, s);
  s->v.while_stmt.break_label_idx = i.v.l.break_idx;
  s->v.while_stmt.continue_label_idx = i.v.l.continue_idx;

  loop_resolve_stmt(lr, s->v.while_stmt.s);

  exit_loop(lr, s);
}

static void loop_resolve_dowhile_stmt(loop_resolver *lr, stmt *s) {
  loop_resolve_info i = enter_loop(lr, s);
  s->v.dowhile_stmt.break_label_idx = i.v.l.break_idx;
  s->v.dowhile_stmt.continue_label_idx = i.v.l.continue_idx;

  loop_resolve_stmt(lr, s->v.dowhile_stmt.s);

  exit_loop(lr, s);
}

static void loop_resolve_for_stmt(loop_resolver *lr, stmt *s) {
  loop_resolve_info i = enter_loop(lr, s);
  s->v.dowhile_stmt.break_label_idx = i.v.l.break_idx;
  s->v.dowhile_stmt.continue_label_idx = i.v.l.continue_idx;

  loop_resolve_stmt(lr, s->v.for_stmt.s);

  exit_loop(lr, s);
}

static void loop_resolve_switch_stmt(loop_resolver *lr, stmt *s) {
  loop_resolve_info i;

  i.t = LOOP_RESOLVE_SWITCH;
  i.s = s;
  i.v.s.default_stmt = NULL;
  s->v.switch_stmt.break_label_idx = i.v.s.break_idx = ++label_idx_counter;
  i.v.s.cases = ht_create();
  i.v.s.cond_type = s->v.switch_stmt.e->tp;

  vec_push_back(lr->stack_loop_resolve_info, i);

  loop_resolve_stmt(lr, s->v.switch_stmt.s);

  assert(lr->stack_loop_resolve_info.size >= 0 &&
         lr->stack_loop_resolve_info.data[lr->stack_loop_resolve_info.size - 1]
                 .s == s);
  i = lr->stack_loop_resolve_info.data[lr->stack_loop_resolve_info.size - 1];

  vec_pop_back(lr->stack_loop_resolve_info);

  s->v.switch_stmt.default_stmt = i.v.s.default_stmt;

  hti it = ht_iterator(i.v.s.cases);
  int c = 0;
  while (ht_next(&it)) {
    ++c;
  }

  extern arena ptr_arena; // main.c
  stmt **arr = ARENA_ALLOC_ARRAY(&ptr_arena, stmt *, c);

  it = ht_iterator(i.v.s.cases);
  int j = 0;
  while (ht_next(&it)) {
    arr[j++] = (stmt *)it.value;
  }

  s->v.switch_stmt.cases = arr;
  s->v.switch_stmt.cases_len = c;

  ht_destroy(i.v.s.cases);
}

// try to find last switch in stack_loop_resolve_info, -1 if not found, idx
// otherwise
static int find_last_switch(loop_resolver *lr) {
  for (int i = lr->stack_loop_resolve_info.size - 1; i >= 0; --i) {
    if (lr->stack_loop_resolve_info.data[i].t == LOOP_RESOLVE_SWITCH)
      return i;
  }

  return -1;
}

// try to find last loop in stack_loop_resolve_info, -1 if not found, idx
// otherwise
static int find_last_loop(loop_resolver *lr) {
  for (int i = lr->stack_loop_resolve_info.size - 1; i >= 0; --i) {
    if (lr->stack_loop_resolve_info.data[i].t == LOOP_RESOLVE_LOOP)
      return i;
  }

  return -1;
}

static void loop_resolve_break_stmt(loop_resolver *lr, stmt *s) {
  if (lr->stack_loop_resolve_info.size <= 0) {
    fprintf(stderr, "can't use break outside of loop or switch (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i =
      &lr->stack_loop_resolve_info.data[lr->stack_loop_resolve_info.size - 1];

  switch (i->t) {
  case LOOP_RESOLVE_LOOP:
    s->v.break_stmt.idx = i->v.l.break_idx;
    break;
  case LOOP_RESOLVE_SWITCH:
    s->v.break_stmt.idx = i->v.s.break_idx;
    break;
  }
}

static void loop_resolve_continue_stmt(loop_resolver *lr, stmt *s) {
  int loop_idx = find_last_loop(lr);

  if (lr->stack_loop_resolve_info.size <= 0 || loop_idx == -1) {
    fprintf(stderr, "can't use continue outside of loop (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i = &lr->stack_loop_resolve_info.data[loop_idx];

  s->v.continue_stmt.idx = i->v.l.continue_idx;
}

static void loop_resolve_default_stmt(loop_resolver *lr, stmt *s) {
  int switch_idx = find_last_switch(lr);
  if (lr->stack_loop_resolve_info.size <= 0 || switch_idx == -1) {
    fprintf(stderr, "can't use default outside of switch (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i = &lr->stack_loop_resolve_info.data[switch_idx];

  if (i->v.s.default_stmt != NULL) {
    stmt *old = i->v.s.default_stmt;
    fprintf(stderr, "default already defined at %d:%d-%d:%d (%d:%d-%d:%d)\n",
            old->pos.line_start, old->pos.pos_start, old->pos.line_end,
            old->pos.pos_end, s->pos.line_start, s->pos.pos_start,
            s->pos.line_end, s->pos.pos_end);

    exit(1);
  }

  i->v.s.default_stmt = s;
  s->v.default_stmt.label_idx = ++label_idx_counter;
}

static void loop_resolve_case_stmt(loop_resolver *lr, stmt *s) {

  int switch_idx = find_last_switch(lr);

  if (lr->stack_loop_resolve_info.size <= 0 || switch_idx == -1) {
    fprintf(stderr, "can't use case outside of switch (%d:%d-%d:%d)\n",
            s->pos.line_start, s->pos.pos_start, s->pos.line_end,
            s->pos.pos_end);

    exit(1);
  }

  loop_resolve_info *i = &lr->stack_loop_resolve_info.data[switch_idx];
  assert(s->v.case_stmt.e->t == EXPR_INT_CONST); // TODO: eval here or smth
  convert_const(s->v.case_stmt.e->v.intc, i->v.s.cond_type);

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

  s->v.case_stmt.label_idx = ++label_idx_counter;
}

static void loop_resolve_stmt(loop_resolver *lr, stmt *s) {
  switch (s->t) {
  case STMT_RETURN:
    break;
  case STMT_BLOCK:
    loop_resolve_block_stmt(lr, s);
    break;
  case STMT_EXPR:
    break;
  case STMT_NULL:
    break;
  case STMT_IF:
    loop_resolve_stmt(lr, s->v.if_stmt.then);
    if (s->v.if_stmt.elze != NULL)
      loop_resolve_stmt(lr, s->v.if_stmt.elze);
    break;
  case STMT_GOTO:
    break;
  case STMT_LABEL:
    loop_resolve_stmt(lr, s->v.label.s);
    break;
  case STMT_WHILE:
    loop_resolve_while_stmt(lr, s);
    break;
  case STMT_DOWHILE:
    loop_resolve_dowhile_stmt(lr, s);
    break;
  case STMT_FOR:
    loop_resolve_for_stmt(lr, s);
    break;
  case STMT_BREAK:
    loop_resolve_break_stmt(lr, s);
    break;
  case STMT_CONTINUE:
    loop_resolve_continue_stmt(lr, s);
    break;
  case STMT_CASE:
    loop_resolve_case_stmt(lr, s);
    break;
  case STMT_DEFAULT:
    loop_resolve_default_stmt(lr, s);
    break;
  case STMT_SWITCH:
    loop_resolve_switch_stmt(lr, s);
    break;
  }
}

static void loop_resolve_decl(loop_resolver *lr, decl *d) {
  if (d->t == DECL_FUNC && d->v.func.bs != NULL) {
    vec_init(lr->stack_loop_resolve_info);
    loop_resolve_block_stmt(lr, d->v.func.bs);
    vec_free(lr->stack_loop_resolve_info);
  }
}

void label_loop(program *p) {
  loop_resolver lr;
  for (decl *d = p->first_decl; d != NULL; d = d->next) {
    loop_resolve_decl(&lr, d);
  }
}
