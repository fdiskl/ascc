#include "parser.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "scan.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

  INIT_ARENA(&p->decl_arena, decl);
  INIT_ARENA(&p->stmt_arena, stmt);
  INIT_ARENA(&p->expr_arena, expr);
  INIT_ARENA(&p->idente_arena, idente);
  INIT_ARENA(&p->bi_arena, idente);

  vec_push_back(arenas_to_free, &p->decl_arena);
  vec_push_back(arenas_to_free, &p->stmt_arena);
  vec_push_back(arenas_to_free, &p->expr_arena);
  vec_push_back(arenas_to_free, &p->idente_arena);
  vec_push_back(arenas_to_free, &p->bi_arena);

  p->ident_ht_list_head = NULL;

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

void resolve_expr(parser *p, expr *e);
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
  case TOK_EXCL:
    e->v.u.t = UNARY_NOT;
    break;
  default:
    UNREACHABLE();
  }

  advance(p);

  e->v.u.e = parse_factor(p);

  return e;
}

static expr *parse_var_expr(parser *p) {
  expr *e = alloc_expr(p, EXPR_VAR);
  e->v.var.name = expect(p, TOK_IDENT)->v.ident;
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
  case TOK_EXCL:
    e = parse_unary_expr(p);
    break;
  case TOK_LPAREN:
    expect(p, TOK_LPAREN);
    e = parse_expr(p);
    expect(p, TOK_RPAREN);
    break;
  case TOK_IDENT:
    e = parse_var_expr(p);
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
  case TOK_ASSIGN:
    return 6;
  case TOK_DOUBLE_PIPE:
    return 8;
  case TOK_DOUBLE_AMP:
    return 9;
  case TOK_PIPE:
    return 10;
  case TOK_CARRET:
    return 11;
  case TOK_AMPER:
    return 12;
  case TOK_EQ:
  case TOK_NE:
    return 13;
  case TOK_LT:
  case TOK_GT:
  case TOK_LE:
  case TOK_GE:
    return 14;
  case TOK_LSHIFT:
  case TOK_RSHIFT:
    return 15;
  case TOK_PLUS:
  case TOK_MINUS:
    return 16;
  case TOK_STAR:
  case TOK_SLASH:
  case TOK_MOD:
    return 17;

  default:
    return 0;
  }
}

// converts token into bin op, 0 if not bin op
static int get_bin_op(parser *p, int t) {
#define b(tok, op)                                                             \
  case tok:                                                                    \
    expect(p, tok);                                                            \
    return op

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
    b(TOK_DOUBLE_AMP, BINARY_AND);
    b(TOK_DOUBLE_PIPE, BINARY_OR);
    b(TOK_EQ, BINARY_EQ);
    b(TOK_NE, BINARY_NE);
    b(TOK_LT, BINARY_LT);
    b(TOK_GT, BINARY_GT);
    b(TOK_LE, BINARY_LE);
    b(TOK_GE, BINARY_GE);
  }

  return 0;
}

static expr *_parse_expr(parser *p, int min_prec) {
  expr *l = parse_factor(p);

  int prec;
  while ((prec = binary_op(p->next.token)) != 0 && prec >= min_prec) {
    int binop = get_bin_op(p, p->next.token);

    // binary
    if (binop) {
      expr *r = _parse_expr(p, prec + 1);

      expr *tmp = l;
      l = alloc_expr(p, EXPR_BINARY);
      l->v.b.l = tmp;
      l->v.b.r = r;
      l->v.b.t = binop;
      SET_POS_FROM_NODES(l, tmp, r);
    } else { // assignment
      expect(p, TOK_ASSIGN);
      expr *r = _parse_expr(p, prec);

      expr *tmp = l;
      l = alloc_expr(p, EXPR_ASSIGNMENT);
      l->v.assignment.l = tmp;
      l->v.assignment.r = r;
      SET_POS_FROM_NODES(l, tmp, r);
    }
  }

  return l;
}

static expr *parse_expr(parser *p) {
  expr *e = _parse_expr(p, MIN_PREC);
  resolve_expr(p, e);
  return e;
}

// returns true if given token is start of declaration
static bool is_decl(int toktype) {
  switch (toktype) {
  case TOK_INT:
    return true;
  default:
    return false;
  }
}

static stmt *parse_stmt(parser *p);
static decl *parse_decl(parser *p);

static block_item parse_bi(parser *p) {
  block_item res;
  if (is_decl(p->next.token)) {
    res.d = parse_decl(p);
    res.s = NULL;
  } else {
    res.d = NULL;
    res.s = parse_stmt(p);
  }

  return res;
}

void enter_scope(parser *p);
void exit_scope(parser *p);

static stmt *parse_block_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_BLOCK);
  expect(p, TOK_LBRACE);

  // not ideal with all this tmp vec, but ok for now
  VEC(block_item) items_tmp;
  vec_init(items_tmp);

  enter_scope(p);

  while (p->next.token != TOK_RBRACE)
    vec_push_back(items_tmp, parse_bi(p));

  exit_scope(p);

  s->v.block.items_len = items_tmp.size;
  vec_move_into_arena(&p->bi_arena, items_tmp, block_item, s->v.block.items);

  vec_free(items_tmp);

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

static stmt *parse_expr_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_EXPR);
  s->v.e = parse_expr(p);
  expect(p, TOK_SEMI);
  return s;
}

static stmt *parse_null_stmt(parser *p) {
  expect(p, TOK_SEMI);
  return alloc_stmt(p, STMT_NULL);
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
  case TOK_SEMI:
    res = parse_null_stmt(p);
    break;
  default:
    res = parse_expr_stmt(p);
  }

  SET_POS(res, start, p->curr.pos);
  return res;
}

void resolve_decl(parser *p, decl *d);

static decl *parse_decl(parser *p) {
  tok_pos start = expect(p, TOK_INT)->pos; // TODO: parse return type of func or
                                           // type of var, not just int
  string ident = expect(p, TOK_IDENT)->v.ident;

  decl *res;

  if (p->next.token == TOK_LPAREN) {
    res = alloc_decl(p, DECL_FUNC);
    res->v.func.name = ident;

    expect(p, TOK_LPAREN);
    expect(p, TOK_VOID); // TODO: parse arg types
    expect(p, TOK_RPAREN);

    expect(p, TOK_LBRACE);

    VEC(block_item) items_tmp;
    vec_init(items_tmp);

    enter_scope(p);

    while (p->next.token != TOK_RBRACE) {
      vec_push_back(items_tmp, parse_bi(p));
    }

    exit_scope(p);

    res->v.func.body_len = items_tmp.size;
    vec_move_into_arena(&p->bi_arena, items_tmp, block_item, res->v.func.body);

    vec_free(items_tmp);

    tok_pos end = expect(p, TOK_RBRACE)->pos;
    SET_POS(res, start, end);

  } else {
    res = alloc_decl(p, DECL_VAR);
    res->v.var.name = ident;
    if (p->next.token == TOK_ASSIGN) {
      expect(p, TOK_ASSIGN);
      res->v.var.init = parse_expr(p);
    } else
      res->v.var.init = NULL;

    tok_pos end = expect(p, TOK_SEMI)->pos;
    SET_POS(res, start, end);
  }

  resolve_decl(p, res);

  return res;
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
