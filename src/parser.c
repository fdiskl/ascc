#include "parser.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "scan.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static token *advance(parser *p) {
  p->curr = p->next;
  next(p->l, &p->next);
  return &p->curr;
}

static token *expect(parser *p, int t) {
  if (p->next.token == t)
    return advance(p);
  fprintf(stderr, "expected %s, found %s", token_name(t),
          token_name(p->next.token));
  after_error();
  return NULL;
}

void init_parser(parser *p, lexer *l) {
  p->l = l;

  init_arena(&p->decl_arena);
  init_arena(&p->stmt_arena);
  init_arena(&p->expr_arena);
  ADD_TO_CLEANUP_ARRAY(arenas_to_free, &p->decl_arena);
  ADD_TO_CLEANUP_ARRAY(arenas_to_free, &p->stmt_arena);
  ADD_TO_CLEANUP_ARRAY(arenas_to_free, &p->expr_arena);

  advance(p);
}

static expr *alloc_expr(parser *p, int t) {
  expr *e = ARENA_ALLOC_OBJ(&p->expr_arena, expr);
  e->t = t;
  return e;
}

static stmt *alloc_stmt(parser *p, int t) {
  stmt *s = ARENA_ALLOC_OBJ(&p->stmt_arena, stmt);
  s->t = t;
  return s;
}

static decl *alloc_decl(parser *p, int t) {
  decl *d = ARENA_ALLOC_OBJ(&p->decl_arena, decl);
  d->next = NULL;
  d->t = t;
  return d;
}

static expr *parse_int_const_expr(parser *p) {
  expr *e = alloc_expr(p, EXPR_INT_CONST);
  e->intc.v = expect(p, TOK_INTLIT)->v.int_val;
  return e;
}

static expr *parse_expr(parser *p) {

  // for now only int const
  return parse_int_const_expr(p);
}

static stmt *parse_stmt(parser *p);

static stmt *parse_block_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_BLOCK);
  expect(p, TOK_LBRACE);
  int i = 0;
  while (p->next.token != TOK_RBRACE)
    vec_push_back(s->v.block.stmts, parse_stmt(p));
  expect(p, TOK_RBRACE);

  return s;
}

static stmt *parse_return_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_RETURN);
  expect(p, TOK_RETURN);
  s->v.ret.e = parse_expr(p);
  expect(p, TOK_SEMI);
  return s;
}

static stmt *parse_stmt(parser *p) {
  switch (p->next.token) {
  case TOK_LBRACE:
    return parse_block_stmt(p);
  case TOK_RETURN:
    return parse_return_stmt(p);
  default:
    assert(0 && "todo"); // TODO: parse expr
  }
}

static decl *parse_decl(parser *p) {
  expect(p, TOK_INT); // TODO: parse return type
  string ident = expect(p, TOK_IDENT)->v.ident;

  if (p->next.token == TOK_LPAREN) {
    decl *res = alloc_decl(p, DECL_FUNC);
    res->v.func.name = ident;

    expect(p, TOK_LPAREN);
    expect(p, TOK_VOID); // TODO: parse arg types
    expect(p, TOK_RPAREN);

    expect(p, TOK_LBRACE);
    int i = 0;
    while (p->next.token != TOK_RBRACE) {
      vec_push_back(res->v.func.body, parse_stmt(p));
    }

    expect(p, TOK_RBRACE);

    return res;
  } else {
    // TODO: var decl
    assert(0 && "todo");
  }
}

program *parse(parser *p) {
  decl *head = NULL;
  decl *tail = NULL;

  while (p->next.token != TOK_EOF) {
    decl *d = parse_decl(p);
    if (head == NULL)
      head = d;
    else
      tail->next = d;

    tail = d;
  }

  return head;
}
