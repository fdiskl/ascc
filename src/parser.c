#include "parser.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "scan.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// binary_op returns 0 on non-bin op, so MIN_PREC should be <= -1 so when
// comparing with >= it still would work
#define MIN_PREC -100

#define SET_POS(n, start, end)                                                 \
  {                                                                            \
    n->pos.filename = start.filename;                                          \
    n->pos.line_start = start.line;                                            \
    n->pos.line_end = end.line;                                                \
    n->pos.pos_start = start.start_pos;                                        \
    n->pos.pos_end = end.end_pos;                                              \
  }

#define SET_POS_FROM_NODES(n, startn, endn)                                    \
  {                                                                            \
    n->pos = startn->pos;                                                      \
    n->pos.line_end = endn->pos.line_end;                                      \
    n->pos.pos_end = endn->pos.pos_end;                                        \
  }

static token *advance(parser *p) {
  p->curr = p->next;
  next(p->l, &p->next);
  return &p->curr;
}

static token *expect(parser *p, int t) {
  if (p->next.token == t)
    return advance(p);
  fprintf(stderr, "expected %s, found %s at [%d:%d:%d]\n", token_name(t),
          token_name(p->next.token), p->next.pos.line, p->next.pos.start_pos,
          p->next.pos.end_pos);
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
  e->v.intc.v = expect(p, TOK_INTLIT)->v.int_val;
  return e;
}

static expr *parse_expr(parser *p);

static expr *parse_factor(parser *p);

static expr *parse_unary_expr(parser *p) {
  expr *e = alloc_expr(p, EXPR_UNARY);
  switch (p->next.token) {
  case TOK_TILDE:
    e->v.u.t = UNARY_COMPLEMENT;
    break;
  case TOK_MINUS:
    e->v.u.t = UNARY_NEGATE;
    break;
  default:
    unreachable();
  }

  advance(p);

  e->v.u.e = parse_factor(p);

  return e;
}

static expr *parse_factor(parser *p) {
  tok_pos start = p->next.pos;
  expr *e;
  switch (p->next.token) {
  case TOK_INTLIT:
    e = parse_int_const_expr(p);
    break;
  case TOK_TILDE:
  case TOK_MINUS:
    e = parse_unary_expr(p);
    break;
  case TOK_LPAREN:
    expect(p, TOK_LPAREN);
    e = parse_expr(p);
    expect(p, TOK_RPAREN);
    break;
  default:
    fprintf(stderr, "invalid token found %s (%d:%d:%d)\n",
            token_name(p->next.token), p->next.pos.line, p->next.pos.start_pos,
            p->next.pos.end_pos); // FIXME: idk, dont really like how it is
    after_error();
    return NULL;
  }

  SET_POS(e, start, p->curr.pos);
  return e;
}

// returns precedence or 0 if not binary op
static int binary_op(int t) {
  switch (t) {
  case TOK_PIPE:
    return 50;
  case TOK_CARRET:
    return 60;
  case TOK_AMPER:
    return 70;
  case TOK_LSHIFT:
  case TOK_RSHIFT:
    return 80;
  case TOK_PLUS:
  case TOK_MINUS:
    return 90;
  case TOK_STAR:
  case TOK_SLASH:
  case TOK_MOD:
    return 100;

  default:
    return 0;
  }
}

// converts token into bin op
static int get_bin_op(parser *p, int t) {
#define b(tok, op)                                                             \
  case tok:                                                                    \
    expect(p, tok);                                                            \
    return op;

  switch (t) {
    b(TOK_PLUS, BINARY_ADD);
    b(TOK_MINUS, BINARY_SUB);
    b(TOK_STAR, BINARY_MUL);
    b(TOK_SLASH, BINARY_DIV);
    b(TOK_MOD, BINARY_MOD);
    b(TOK_AMPER, BINARY_BITWISE_AND);
    b(TOK_PIPE, BINARY_BITWISE_OR);
    b(TOK_CARRET, BINARY_XOR);
    b(TOK_LSHIFT, BINARY_LSHIFT);
    b(TOK_RSHIFT, BINARY_RSHIFT);
  default:
    unreachable(); // should be, at least :)
  }
}

static expr *_parse_expr(parser *p, int min_prec) {
  expr *l = parse_factor(p);

  int prec;
  while ((prec = binary_op(p->next.token)) != 0 && prec >= min_prec) {
    int binop = get_bin_op(p, p->next.token);
    expr *r = _parse_expr(p, prec + 1);

    expr *tmp = l;
    l = alloc_expr(p, EXPR_BINARY);
    l->v.b.l = tmp;
    l->v.b.r = r;
    l->v.b.t = binop;
    SET_POS_FROM_NODES(l, tmp, r);
  }

  return l;
}

static expr *parse_expr(parser *p) { return _parse_expr(p, MIN_PREC); }

static stmt *parse_stmt(parser *p);

static stmt *parse_block_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_BLOCK);
  expect(p, TOK_LBRACE);
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
  tok_pos start = p->next.pos;
  stmt *res;
  switch (p->next.token) {
  case TOK_LBRACE:
    res = parse_block_stmt(p);
    break;
  case TOK_RETURN:
    res = parse_return_stmt(p);
    break;
  default:
    assert(0 && "todo"); // TODO: parse expr
  }

  SET_POS(res, start, p->curr.pos);
  return res;
}

static decl *parse_decl(parser *p) {
  tok_pos start = expect(p, TOK_INT)->pos; // TODO: parse return type
  string ident = expect(p, TOK_IDENT)->v.ident;

  if (p->next.token == TOK_LPAREN) {
    decl *res = alloc_decl(p, DECL_FUNC);
    res->v.func.name = ident;

    expect(p, TOK_LPAREN);
    expect(p, TOK_VOID); // TODO: parse arg types
    expect(p, TOK_RPAREN);

    expect(p, TOK_LBRACE);
    while (p->next.token != TOK_RBRACE) {
      vec_push_back(res->v.func.body, parse_stmt(p));
    }

    tok_pos end = expect(p, TOK_RBRACE)->pos;
    SET_POS(res, start, end);

    return res;
  } else {
    todo();
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
