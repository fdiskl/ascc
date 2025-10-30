#include "parser.h"
#include "arena.h"
#include "common.h"
#include "scan.h"
#include "table.h"
#include "type.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO RN: types in params, return types

// TODO: location for block stmt

extern arena ptr_arena; // in main.c

// binary_op returns 0 on non-bin op, so MIN_PREC should be <= -1 so when
// comparing with >= it still would work
#define MIN_PREC -100

#define SET_POS(n, start, end)                                                 \
  do {                                                                         \
    n->pos.filename = start.filename;                                          \
    n->pos.line_start = start.line;                                            \
    n->pos.line_end = end.line;                                                \
    n->pos.pos_start = start.start_pos;                                        \
    n->pos.pos_end = end.end_pos;                                              \
  } while (0)

#define CONVERT_POS(start, end, ast_pos)                                       \
  do {                                                                         \
    ast_pos.filename = start.filename;                                         \
    ast_pos.line_start = start.line;                                           \
    ast_pos.line_end = end.line;                                               \
    ast_pos.pos_start = start.start_pos;                                       \
    ast_pos.pos_end = end.end_pos;                                             \
  } while (0)

#define SET_POS_FROM_NODES(n, startn, endn)                                    \
  do {                                                                         \
    n->pos = startn->pos;                                                      \
    n->pos.line_end = endn->pos.line_end;                                      \
    n->pos.pos_end = endn->pos.pos_end;                                        \
  } while (0)

static token *advance(parser *p) {
  p->curr = p->next;
  p->next = p->after_next;
  next(p->l, &p->after_next);
  return &p->curr;
}

static token *expect(parser *p, int t) {
  if (p->next.token == t)
    return advance(p);
  fprintf(stderr, "expected %s, found %s at [%d:%d:%d]\n", token_name(t),
          token_name(p->next.token), p->next.pos.line, p->next.pos.start_pos,
          p->next.pos.end_pos);
  exit(1);
  return NULL;
}

static void init_parser(parser *p, lexer *l) {
  p->l = l;

  NEW_ARENA(p->decl_arena, decl);
  NEW_ARENA(p->stmt_arena, stmt);
  NEW_ARENA(p->expr_arena, expr);
  NEW_ARENA(p->bi_arena, block_item);

  INIT_ARENA(&p->ident_entry_arena, ident_entry);

  p->ident_ht_list_head = ht_create();

  advance(p); // for after_next
  advance(p); // for next
}

static void free_parser(parser *p) {
  for (ht *h = p->ident_ht_list_head; h != NULL; h = ht_get_next_table(h))
    ht_destroy(h);

  free_arena(&p->ident_entry_arena);
}

static expr *alloc_expr(parser *p, int t) {
  expr *e = ARENA_ALLOC_OBJ(p->expr_arena, expr);
  e->t = t;
  e->tp = NULL;
  return e;
}

static stmt *alloc_stmt(parser *p, int t) {
  stmt *s = ARENA_ALLOC_OBJ(p->stmt_arena, stmt);
  s->t = t;
  return s;
}

static decl *alloc_decl(parser *p, int t) {
  decl *d = ARENA_ALLOC_OBJ(p->decl_arena, decl);
  d->next = NULL;
  d->t = t;
  return d;
}

static expr *parse_int_const_expr(parser *p) {
  expr *e = alloc_expr(p, EXPR_INT_CONST);

  int_literal v;

  if (p->next.token == TOK_INTLIT)
    v = expect(p, TOK_INTLIT)->v.int_lit;
  else if (p->next.token == TOK_LONGLIT)
    v = expect(p, TOK_LONGLIT)->v.int_lit;

  e->v.intc.v = v.v;

  if (v.v > (1ULL << 63) - 1) //  1ULL << 63 = 2^63
  {
    fprintf(stderr, "constant is too big (%d:%d:%d)\n", p->curr.pos.line,
            p->curr.pos.start_pos, p->curr.pos.end_pos);
    exit(1);
  }

  if (v.suff == INT_SUFF_NONE && v.v <= (1ULL << 31) - 1) {
    e->v.intc.t = CONST_INT;
  } else {
    e->v.intc.t = CONST_LONG;
  }

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
  case TOK_DOUBLE_PLUS:
    e->v.u.t = UNARY_PREFIX_INC;
    break;
  case TOK_DOUBLE_MINUS:
    e->v.u.t = UNARY_PREFIX_DEC;
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
  e->v.var.original_name = expect(p, TOK_IDENT)->v.ident;
  return e;
}

static expr *parse_postfix(parser *p, expr *e) {
  switch (p->next.token) {
  case TOK_DOUBLE_PLUS:
  case TOK_DOUBLE_MINUS: {
    int t = p->next.token == TOK_DOUBLE_PLUS ? UNARY_POSTFIX_INC
                                             : UNARY_POSTFIX_DEC;

    tok_pos pos = advance(p)->pos; // eat op

    expr *outer = alloc_expr(p, EXPR_UNARY);
    outer->v.u.t = t;
    outer->v.u.e = e;

    outer->pos.filename = e->pos.filename;
    outer->pos.line_start = e->pos.line_start;
    outer->pos.pos_start = e->pos.pos_start;
    outer->pos.line_end = pos.line;
    outer->pos.pos_end = pos.end_pos;

    return outer;
  }
  default:
    return e;
  }
}

static expr *parse_func_call_expr(parser *p) {
  expr *e = alloc_expr(p, EXPR_FUNC_CALL);

  e->v.func_call.name = expect(p, TOK_IDENT)->v.ident;
  expect(p, TOK_LPAREN);

  if (p->next.token != TOK_RPAREN) {
    VEC(expr *) args;
    vec_init(args);

    vec_push_back(args, parse_expr(p));

    while (p->next.token != TOK_RPAREN) {
      expect(p, TOK_COMMA);
      vec_push_back(args, parse_expr(p));
    }

    e->v.func_call.args_len = args.size;
    vec_move_into_arena(&ptr_arena, args, expr *, e->v.func_call.args);

    vec_free(args);
  }

  expect(p, TOK_RPAREN);

  return e;
}

static type *parse_type(parser *p);

static expr *parse_type_casting(parser *p) {
  // lparen is already parsed

  expr *res = alloc_expr(p, EXPR_CAST);

  type *t = parse_type(p);

  res->v.cast.tp = t;

  expect(p, TOK_RPAREN);

  res->v.cast.e = parse_factor(p);

  return res;
}

static bool is_decl(int toktype);

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
  case TOK_DOUBLE_PLUS:
  case TOK_DOUBLE_MINUS:
    e = parse_unary_expr(p);
    break;
  case TOK_LPAREN:
    expect(p, TOK_LPAREN);
    if (is_decl(p->next.token)) {
      // type casting
      e = parse_type_casting(p);
      break;
    } else {
      // just parens
      e = parse_expr(p);
      expect(p, TOK_RPAREN);
      break;
    }
  case TOK_IDENT:
    if (p->after_next.token == TOK_LPAREN)
      e = parse_func_call_expr(p);
    else
      e = parse_var_expr(p);
    break;
  default:
    fprintf(stderr, "invalid token found - %s (%d:%d:%d)\n",
            token_name(p->next.token), p->next.pos.line, p->next.pos.start_pos,
            p->next.pos.end_pos); // FIXME: idk, dont really like how it is
    exit(1);
    return NULL;
  }

  SET_POS(e, start, p->curr.pos);
  return parse_postfix(p, e);
}

// returns precedence or 0 if not binary op
static int binary_op(int t) {
  switch (t) {
  case TOK_ASSIGN:
  case TOK_ASPLUS:
  case TOK_ASMINUS:
  case TOK_ASSTAR:
  case TOK_ASSLASH:
  case TOK_ASPERCENT:
  case TOK_ASLSHIFT:
  case TOK_ASRSHIFT:
  case TOK_ASAMP:
  case TOK_ASCARET:
  case TOK_ASPIPE:
    return 6;
  case TOK_QUESTION:
    return 7;
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

#define TOK_INTO_OP(tok, op)                                                   \
  case tok:                                                                    \
    expect(p, tok);                                                            \
    return op

// converts token into bin op, 0 if not bin op
static int get_bin_op(parser *p, int t) {
  switch (t) {
    TOK_INTO_OP(TOK_PLUS, BINARY_ADD);
    TOK_INTO_OP(TOK_MINUS, BINARY_SUB);
    TOK_INTO_OP(TOK_STAR, BINARY_MUL);
    TOK_INTO_OP(TOK_SLASH, BINARY_DIV);
    TOK_INTO_OP(TOK_MOD, BINARY_MOD);
    TOK_INTO_OP(TOK_AMPER, BINARY_BITWISE_AND);
    TOK_INTO_OP(TOK_PIPE, BINARY_BITWISE_OR);
    TOK_INTO_OP(TOK_CARRET, BINARY_XOR);
    TOK_INTO_OP(TOK_LSHIFT, BINARY_LSHIFT);
    TOK_INTO_OP(TOK_RSHIFT, BINARY_RSHIFT);
    TOK_INTO_OP(TOK_DOUBLE_AMP, BINARY_AND);
    TOK_INTO_OP(TOK_DOUBLE_PIPE, BINARY_OR);
    TOK_INTO_OP(TOK_EQ, BINARY_EQ);
    TOK_INTO_OP(TOK_NE, BINARY_NE);
    TOK_INTO_OP(TOK_LT, BINARY_LT);
    TOK_INTO_OP(TOK_GT, BINARY_GT);
    TOK_INTO_OP(TOK_LE, BINARY_LE);
    TOK_INTO_OP(TOK_GE, BINARY_GE);
  }

  return 0;
}

// converts token into assignment op, 0 if not bin op
static int get_assignment_op(parser *p, int t) {
  switch (t) {
    TOK_INTO_OP(TOK_ASSIGN, ASSIGN);
    TOK_INTO_OP(TOK_ASPLUS, ASSIGN_ADD);
    TOK_INTO_OP(TOK_ASMINUS, ASSIGN_SUB);
    TOK_INTO_OP(TOK_ASSTAR, ASSIGN_MUL);
    TOK_INTO_OP(TOK_ASSLASH, ASSIGN_DIV);
    TOK_INTO_OP(TOK_ASPERCENT, ASSIGN_MOD);
    TOK_INTO_OP(TOK_ASLSHIFT, ASSIGN_LSHIFT);
    TOK_INTO_OP(TOK_ASRSHIFT, ASSIGN_RSHIFT);
    TOK_INTO_OP(TOK_ASAMP, ASSIGN_AND);
    TOK_INTO_OP(TOK_ASPIPE, ASSIGN_OR);
    TOK_INTO_OP(TOK_ASCARET, ASSIGN_XOR);
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
      continue;
    }

    int assignop = get_assignment_op(p, p->next.token);

    // assignment
    if (assignop) {
      expr *r = _parse_expr(p, prec);

      expr *tmp = l;
      l = alloc_expr(p, EXPR_ASSIGNMENT);
      l->v.assignment.l = tmp;
      l->v.assignment.r = r;
      l->v.assignment.t = assignop;

      SET_POS_FROM_NODES(l, tmp, r);

      continue;
    }

    // ternary
    expect(p, TOK_QUESTION);
    expr *mid = parse_expr(p);
    expect(p, TOK_COLON);
    expr *right = _parse_expr(p, prec);
    expr *tmp = l;
    l = alloc_expr(p, EXPR_TERNARY);
    l->v.ternary.cond = tmp;
    l->v.ternary.then = mid;
    l->v.ternary.elze = right;

    SET_POS_FROM_NODES(l, tmp, right);
  }

  return l;
}

static expr *parse_expr(parser *p) {
  expr *e = _parse_expr(p, MIN_PREC);
  resolve_expr(p, e);
  return e;
}

static expr *parse_constant_expr(parser *p) {
  expr *e = _parse_expr(p, MIN_PREC);
  check_for_constant_expr(e);
  return e;
}

// returns true if given token is start of declaration
static bool is_decl(int toktype) {
  switch (toktype) {
  case TOK_INT:
  case TOK_STATIC:
  case TOK_EXTERN:
  case TOK_LONG:
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
  vec_move_into_arena(p->bi_arena, items_tmp, block_item, s->v.block.items);

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

static stmt *parse_if_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_IF);
  expect(p, TOK_IF);
  expect(p, TOK_LPAREN);
  s->v.if_stmt.cond = parse_expr(p);
  expect(p, TOK_RPAREN);
  s->v.if_stmt.then = parse_stmt(p);
  if (p->next.token == TOK_ELSE) {
    expect(p, TOK_ELSE);
    s->v.if_stmt.elze = parse_stmt(p);
  } else
    s->v.if_stmt.elze = NULL;
  return s;
}

void resolve_label_stmt(parser *p, stmt *s);
void resolve_goto_stmt(parser *p, stmt *s);

static stmt *parse_label_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_LABEL);
  s->v.label.label = expect(p, TOK_IDENT)->v.ident;
  expect(p, TOK_COLON);
  s->v.label.s = parse_stmt(p);

  resolve_label_stmt(p, s);

  return s;
}

static stmt *parse_goto_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_GOTO);
  expect(p, TOK_GOTO);
  s->v.goto_stmt.label = expect(p, TOK_IDENT)->v.ident;
  expect(p, TOK_SEMI);

  resolve_goto_stmt(p, s);

  return s;
}

static stmt *parse_while_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_WHILE);
  expect(p, TOK_WHILE);
  expect(p, TOK_LPAREN);
  s->v.while_stmt.cond = parse_expr(p);
  expect(p, TOK_RPAREN);
  s->v.while_stmt.s = parse_stmt(p);
  s->v.while_stmt.break_label_idx = -1;
  s->v.while_stmt.continue_label_idx = -1;
  return s;
}

static stmt *parse_do_while_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_DOWHILE);
  expect(p, TOK_DO);
  s->v.dowhile_stmt.s = parse_stmt(p);
  expect(p, TOK_WHILE);
  expect(p, TOK_LPAREN);
  s->v.dowhile_stmt.cond = parse_expr(p);
  expect(p, TOK_RPAREN);
  expect(p, TOK_SEMI);
  s->v.dowhile_stmt.break_label_idx = -1;
  s->v.dowhile_stmt.continue_label_idx = -1;
  return s;
}

static stmt *parse_for_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_FOR);

  s->v.for_stmt.init_e = NULL;
  s->v.for_stmt.init_d = NULL;
  s->v.for_stmt.cond = NULL;
  s->v.for_stmt.post = NULL;

  expect(p, TOK_FOR);
  expect(p, TOK_LPAREN);

  char decl = false;

  // init
  if (p->next.token != TOK_SEMI) {
    if (is_decl(p->next.token)) {
      decl = true;
      enter_scope(p);
      s->v.for_stmt.init_d = parse_decl(p);
      if (s->v.for_stmt.init_d->t == DECL_FUNC) {
        ast_pos pos = s->v.for_stmt.init_d->pos;
        fprintf(stderr,
                "function declaration are not permitted in for loop init "
                "(%d:%d-%d:%d)\n",
                pos.line_start, pos.pos_start, pos.line_end, pos.pos_end);
        exit(1);
      }
      goto after_semi;
    } else
      s->v.for_stmt.init_e = parse_expr(p);
  }

  expect(p, TOK_SEMI);
after_semi:

  // cond
  if (p->next.token != TOK_SEMI)
    s->v.for_stmt.cond = parse_expr(p);

  expect(p, TOK_SEMI);

  // post
  if (p->next.token != TOK_RPAREN)
    s->v.for_stmt.post = parse_expr(p);

  expect(p, TOK_RPAREN);

  s->v.for_stmt.s = parse_stmt(p);
  if (decl)
    exit_scope(p);

  s->v.for_stmt.break_label_idx = -1;
  s->v.for_stmt.continue_label_idx = -1;

  return s;
}

static stmt *parse_break_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_BREAK);
  expect(p, TOK_BREAK);
  expect(p, TOK_SEMI);
  s->v.break_stmt.idx = -1;
  return s;
}

static stmt *parse_continue_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_CONTINUE);
  expect(p, TOK_CONTINUE);
  expect(p, TOK_SEMI);
  s->v.continue_stmt.idx = -1;
  return s;
}

static stmt *parse_case_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_CASE);
  expect(p, TOK_CASE);
  s->v.case_stmt.e = parse_constant_expr(p);
  expect(p, TOK_COLON);
  s->v.case_stmt.s = parse_stmt(p);
  s->v.case_stmt.label_idx = -1;
  return s;
}

static stmt *parse_default_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_DEFAULT);
  expect(p, TOK_DEFAULT);
  expect(p, TOK_COLON);
  s->v.default_stmt.s = parse_stmt(p);
  s->v.default_stmt.label_idx = -1;
  return s;
}

static stmt *parse_switch_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_SWITCH);
  expect(p, TOK_SWITCH);
  expect(p, TOK_LPAREN);
  s->v.switch_stmt.e = parse_expr(p);
  expect(p, TOK_RPAREN);
  s->v.switch_stmt.s = parse_stmt(p);
  s->v.switch_stmt.break_label_idx = -1;
  s->v.switch_stmt.cases_len = 0;
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
  case TOK_SEMI:
    res = parse_null_stmt(p);
    break;
  case TOK_IF:
    res = parse_if_stmt(p);
    break;
  case TOK_IDENT:
    if (p->after_next.token == TOK_COLON) {
      res = parse_label_stmt(p);
      break;
    }
    goto _default;
    break;
  case TOK_GOTO:
    res = parse_goto_stmt(p);
    break;
  case TOK_WHILE:
    res = parse_while_stmt(p);
    break;
  case TOK_DO:
    res = parse_do_while_stmt(p);
    break;
  case TOK_FOR:
    res = parse_for_stmt(p);
    break;
  case TOK_BREAK:
    res = parse_break_stmt(p);
    break;
  case TOK_CONTINUE:
    res = parse_continue_stmt(p);
    break;
  case TOK_CASE:
    res = parse_case_stmt(p);
    break;
  case TOK_DEFAULT:
    res = parse_default_stmt(p);
    break;
  case TOK_SWITCH:
    res = parse_switch_stmt(p);
    break;
  default:
  _default:
    res = parse_expr_stmt(p);
  }

  SET_POS(res, start, p->curr.pos);
  return res;
}

void enter_func(parser *p, decl *f);          // resolve.c
void enter_body_of_func(parser *p, decl *f);  // resolve.c
void exit_func(parser *p, decl *f);           // resolve.c
void exit_func_with_body(parser *p, decl *f); // resolve.c

ident_entry *resolve_var_decl(parser *p, string name, ast_pos pos, char param,
                              sct sc); // resolve.c

static type *parse_type(parser *p);

static void parse_params(parser *p, func_decl *f, type *ft) {
  if (p->next.token == TOK_VOID) {
    expect(p, TOK_VOID);
    f->params_names = NULL;
    f->original_params = NULL;
    f->params_len = 0;
    ft->v.fntype.params = NULL;
    ft->v.fntype.param_count = 0;
    return;
  }

  VEC(string) params;
  VEC(string) params_new_names;
  VEC(type *) params_types;
  vec_init(params);
  vec_init(params_new_names);
  vec_init(params_types);

  ast_pos pos;
  tok_pos start = p->next.pos;
  type *param_type = parse_type(p);
  tok_pos end = expect(p, TOK_IDENT)->pos;
  CONVERT_POS(start, end, pos);

  vec_push_back(params, p->curr.v.ident);
  vec_push_back(params_new_names,
                resolve_var_decl(p, p->curr.v.ident, pos, true, SC_NONE)->name);
  vec_push_back(params_types, param_type);

  while (p->next.token != TOK_RPAREN) {
    start = expect(p, TOK_COMMA)->pos;
    type *param_type = parse_type(p);
    end = expect(p, TOK_IDENT)->pos;
    CONVERT_POS(start, end, pos);

    vec_push_back(params, p->curr.v.ident);
    vec_push_back(
        params_new_names,
        resolve_var_decl(p, p->curr.v.ident, pos, true, SC_NONE)->name);
    vec_push_back(params_types, param_type);
  }

  ft->v.fntype.param_count = f->params_len = params.size;

  vec_move_into_arena(&ptr_arena, params, string, f->original_params);
  vec_move_into_arena(&ptr_arena, params_new_names, string, f->params_names);
  vec_move_into_arena(&ptr_arena, params_types, type *, ft->v.fntype.params);

  vec_free(params);
  vec_free(params_new_names);
  vec_free(params_types);
}

VEC_T(type_vector_t, int);

static type *convert_type(type_vector_t tokens, tok_pos pos) {
  typet t;
  switch (tokens.size) {
  case 1:
    if (tokens.data[0] == TOK_INT) {
      t = TYPE_INT;
      break;
    }
    if (tokens.data[0] == TOK_LONG) {
      t = TYPE_LONG;
      break;
    }
    goto fail;
    break;
  case 2:
    if (tokens.data[0] == TOK_INT && tokens.data[1] == TOK_LONG) {
      t = TYPE_LONG;
      break;
    }
    if (tokens.data[0] == TOK_LONG && tokens.data[1] == TOK_INT) {
      t = TYPE_LONG;
      break;
    }
    goto fail;
    break;
  default:
    goto fail;
    break;
  }

  return new_type(t);

fail: {
  fprintf(stderr, "invalid type %d:%d\n", pos.line, pos.start_pos);
  exit(1);
  return NULL;
}
}

static type *parse_type(parser *p) {
  type_vector_t types;
  vec_init(types);

  tok_pos pos = p->next.pos;
  while (p->next.token != TOK_IDENT &&
         (p->next.token == TOK_INT || p->next.token == TOK_LONG)) {
    advance(p);
    vec_push_back(types, p->curr.token);
  }

  type *res = convert_type(types, pos);

  vec_free(types);

  return res;
}

// returns storace class and write type into tp_ptr
static sct parse_type_and_storage_class(parser *p, type **tp_ptr) {
  type_vector_t types;
  VEC(int) scs;
  vec_init(types);
  vec_init(scs);

  tok_pos pos = p->next.pos;

  while (p->next.token != TOK_IDENT &&
         (p->next.token == TOK_INT || p->next.token == TOK_STATIC ||
          p->next.token == TOK_EXTERN || p->next.token == TOK_LONG)) {
    advance(p);

    if (p->curr.token == TOK_INT || p->curr.token == TOK_LONG)
      vec_push_back(types, p->curr.token);
    else
      vec_push_back(scs, p->curr.token);
  }

  type *tp = convert_type(types, pos);
  *tp_ptr = tp;

  if (scs.size > 1) {
    fprintf(stderr, "invalid storage class specifier\n");
    exit(1);
  }

  sct res;
  if (scs.size == 0)
    res = SC_NONE;
  else
    res = scs.data[0] == TOK_EXTERN ? SC_EXTERN : SC_STATIC;

  vec_free(types);
  vec_free(scs);

  return res;
}

static decl *parse_decl(parser *p) {
  tok_pos start = p->next.pos;

  type *tp = NULL;
  sct sc = parse_type_and_storage_class(p, &tp);
  string ident = expect(p, TOK_IDENT)->v.ident;
  tok_pos tmp_end = p->curr.pos;

  decl *res;

  if (p->next.token == TOK_LPAREN) {
    res = alloc_decl(p, DECL_FUNC);
    res->v.func.name = ident;

    type *fn_tp = new_type(TYPE_FN);
    fn_tp->v.fntype.return_type = tp;
    res->tp = fn_tp;

    enter_func(p, res);

    expect(p, TOK_LPAREN);

    parse_params(p, &res->v.func, fn_tp);

    expect(p, TOK_RPAREN);

    tok_pos end;

    if (p->next.token == TOK_SEMI) {
      end = expect(p, TOK_SEMI)->pos;
      res->v.func.bs = NULL;
      goto after_body_parse;
    }

    enter_body_of_func(p, res);

    expect(p, TOK_LBRACE);

    VEC(block_item) items_tmp;
    vec_init(items_tmp);

    while (p->next.token != TOK_RBRACE) {
      vec_push_back(items_tmp, parse_bi(p));
    }

    stmt *s;
    res->v.func.bs = s = alloc_stmt(p, STMT_BLOCK);
    s->v.block.items_len = items_tmp.size;
    vec_move_into_arena(p->bi_arena, items_tmp, block_item, s->v.block.items);

    vec_free(items_tmp);

    end = expect(p, TOK_RBRACE)->pos;
  after_body_parse: {
    SET_POS(res, start, end);
    if (res->v.func.bs == NULL)
      exit_func(p, res);
    else
      exit_func_with_body(p, res);
  }

  } else {
    res = alloc_decl(p, DECL_VAR);
    res->v.var.original_name = ident;
    res->tp = tp;

    ast_pos pos;
    CONVERT_POS(start, tmp_end, pos);

    ident_entry *e = resolve_var_decl(p, ident, pos, false, sc);
    res->v.var.name = e->name;
    res->scope = e->scope;

    if (p->next.token == TOK_ASSIGN) {
      expect(p, TOK_ASSIGN);
      res->v.var.init = parse_expr(p);
    } else
      res->v.var.init = NULL;

    tok_pos end = expect(p, TOK_SEMI)->pos;
    SET_POS(res, start, end);
  }

  res->sc = sc;

  return res;
}

program parse(lexer *l) {
  parser p;
  init_parser(&p, l);
  program res;

  decl *head = NULL;
  decl *tail = NULL;

  while (p.next.token != TOK_EOF) {
    decl *d = parse_decl(&p);
    if (head == NULL)
      head = d;
    else
      tail->next = d;

    tail = d;
  }

  res.first_decl = head;
  res.decl_arena = p.decl_arena;
  res.expr_arena = p.expr_arena;
  res.stmt_arena = p.stmt_arena;
  res.bi_arena = p.bi_arena;

  free_parser(&p);

  return res;
}

void free_program(program *p) {
  destroy_arena(p->decl_arena);
  destroy_arena(p->expr_arena);
  destroy_arena(p->bi_arena);
  destroy_arena(p->stmt_arena);
}
