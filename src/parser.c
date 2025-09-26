#include "parser.h"
#include "arena.h"
#include "common.h"
#include "driver.h"
#include "scan.h"
#include "table.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
  after_error();
  return NULL;
}

void init_parser(parser *p, lexer *l) {
  p->l = l;

  INIT_ARENA(&p->decl_arena, decl);
  INIT_ARENA(&p->stmt_arena, stmt);
  INIT_ARENA(&p->expr_arena, expr);
  INIT_ARENA(&p->bi_arena, block_item);
  INIT_ARENA(&p->symbol_arena, ident_entry);

  vec_push_back(arenas_to_free, &p->decl_arena);
  vec_push_back(arenas_to_free, &p->stmt_arena);
  vec_push_back(arenas_to_free, &p->expr_arena);
  vec_push_back(arenas_to_free, &p->bi_arena);
  vec_push_back(arenas_to_free, &p->symbol_arena);

  p->ident_ht_list_head = ht_create();
  p->funcs_ht = ht_create();

  vec_push_back(tables_to_destroy, p->funcs_ht);
  vec_push_back(tables_to_destroy, p->ident_ht_list_head);

  advance(p); // for after_next
  advance(p); // for next
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
  e->v.var.name = expect(p, TOK_IDENT)->v.ident;
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
    e = parse_expr(p);
    expect(p, TOK_RPAREN);
    break;
  case TOK_IDENT:
    if (p->after_next.token == TOK_LPAREN)
      e = parse_func_call_expr(p);
    else
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

void enter_loop(parser *p, stmt *s);
void exit_loop(parser *p, stmt *s);
void enter_switch(parser *p, stmt *s);
void exit_switch(parser *p, stmt *s);

static stmt *parse_while_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_WHILE);
  expect(p, TOK_WHILE);
  expect(p, TOK_LPAREN);
  s->v.while_stmt.cond = parse_expr(p);
  expect(p, TOK_RPAREN);
  enter_loop(p, s);
  s->v.while_stmt.s = parse_stmt(p);
  exit_loop(p, s);
  return s;
}

static stmt *parse_do_while_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_DOWHILE);
  expect(p, TOK_DO);
  enter_loop(p, s);
  s->v.dowhile_stmt.s = parse_stmt(p);
  exit_loop(p, s);
  expect(p, TOK_WHILE);
  expect(p, TOK_LPAREN);
  s->v.dowhile_stmt.cond = parse_expr(p);
  expect(p, TOK_RPAREN);
  expect(p, TOK_SEMI);
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
        after_error();
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

  enter_loop(p, s);
  s->v.for_stmt.s = parse_stmt(p);
  if (decl)
    exit_scope(p);

  exit_loop(p, s);

  return s;
}

void resolve_break_stmt(parser *p, stmt *s);
void resolve_continue_stmt(parser *p, stmt *s);
void resolve_case_stmt(parser *p, stmt *s);
void resolve_default_stmt(parser *p, stmt *s);

static stmt *parse_break_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_BREAK);
  expect(p, TOK_BREAK);
  expect(p, TOK_SEMI);
  resolve_break_stmt(p, s);
  return s;
}

static stmt *parse_continue_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_CONTINUE);
  expect(p, TOK_CONTINUE);
  expect(p, TOK_SEMI);
  resolve_continue_stmt(p, s);
  return s;
}

static stmt *parse_case_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_CASE);
  expect(p, TOK_CASE);
  s->v.case_stmt.e = parse_constant_expr(p);
  expect(p, TOK_COLON);
  s->v.case_stmt.s = parse_stmt(p);
  resolve_case_stmt(p, s);
  return s;
}

static stmt *parse_default_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_DEFAULT);
  expect(p, TOK_DEFAULT);
  expect(p, TOK_COLON);
  s->v.default_stmt.s = parse_stmt(p);
  resolve_default_stmt(p, s);
  return s;
}

static stmt *parse_switch_stmt(parser *p) {
  stmt *s = alloc_stmt(p, STMT_SWITCH);
  expect(p, TOK_SWITCH);
  expect(p, TOK_LPAREN);
  s->v.switch_stmt.e = parse_expr(p);
  expect(p, TOK_RPAREN);
  enter_switch(p, s);
  s->v.switch_stmt.s = parse_stmt(p);
  exit_switch(p, s);
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

static void parse_params(parser *p, func_decl *f) {
  if (p->next.token == TOK_VOID) {
    expect(p, TOK_VOID);
    f->params = NULL;
    f->params_len = 0;
    return;
  }

  VEC(string) params;
  VEC(void *) params_idxs;
  vec_init(params);
  vec_init(params_idxs);

  ast_pos pos;
  tok_pos start = expect(p, TOK_INT)->pos;
  tok_pos end = expect(p, TOK_IDENT)->pos;
  CONVERT_POS(start, end, pos);

  vec_push_back(params, p->curr.v.ident);
  vec_push_back(params_idxs,
                (void *)((intptr_t)resolve_var_decl(p, p->curr.v.ident, pos,
                                                    true, SC_NONE)
                             ->name_idx));

  while (p->next.token != TOK_RPAREN) {
    start = expect(p, TOK_COMMA)->pos;
    expect(p, TOK_INT);
    end = expect(p, TOK_IDENT)->pos;
    CONVERT_POS(start, end, pos);

    vec_push_back(params, p->curr.v.ident);
    vec_push_back(params_idxs,
                  (void *)(intptr_t)(resolve_var_decl(p, p->curr.v.ident, pos,
                                                      true, SC_NONE)
                                         ->name_idx));
  }

  f->params_len = params.size;

  vec_move_into_arena(&ptr_arena, params, string, f->params);
  vec_move_into_arena(&ptr_arena, params_idxs, void *, f->params_idxs);

  vec_free(params);
}

static sct parse_type_and_storage_class(parser *p) {
  VEC(int) types;
  VEC(int) scs;
  vec_init(types);
  vec_init(scs);

  int i = 0;

  while (p->next.token != TOK_IDENT &&
         (p->next.token == TOK_INT || p->next.token == TOK_STATIC ||
          p->next.token == TOK_EXTERN)) {
    advance(p);

    if (p->curr.token == TOK_INT)
      vec_push_back(types, p->curr.token);
    else
      vec_push_back(scs, p->curr.token);
  }

  // TODO: line info in err
  if (types.size != 1) {
    vec_free(types);
    vec_free(scs);
    fprintf(stderr, "invalid type specifier\n");
    after_error();
  }
  if (scs.size > 1) {
    vec_free(types);
    vec_free(scs);
    fprintf(stderr, "invalid storage class specifier\n");
    after_error();
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

  sct sc = parse_type_and_storage_class(p);
  string ident = expect(p, TOK_IDENT)->v.ident;
  tok_pos tmp_end = p->curr.pos;

  decl *res;

  if (p->next.token == TOK_LPAREN) {
    res = alloc_decl(p, DECL_FUNC);
    res->v.func.name = ident;

    enter_func(p, res);

    expect(p, TOK_LPAREN);

    parse_params(p, &res->v.func);

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
    vec_move_into_arena(&p->bi_arena, items_tmp, block_item, s->v.block.items);

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
    res->v.var.name = ident;

    ast_pos pos;
    CONVERT_POS(start, tmp_end, pos);

    ident_entry *e = resolve_var_decl(p, ident, pos, false, sc);
    res->v.var.name_idx = e->name_idx;
    res->v.var.scope = e->scope;

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
