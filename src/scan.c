#include "scan.h"
#include "common.h"
#include "string.h"
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_lexer(lexer *l, FILE *f) {
  l->f = f;
  l->line = 1;
  l->pos = 0;

  l->putback = 0;
}

static char handle_preprocessor_directive(lexer *l);

static char next_char(lexer *l) {
  char c;

  if (l->putback) {
    c = l->putback;
    l->putback = 0;
    return c;
  }

  c = fgetc(l->f);

  if (c != EOF)
    ++l->pos;
  if (c == '\n') {
    ++l->line;
    l->pos = 0;
  }

  if (c == '#')
    return handle_preprocessor_directive(l);

  return c;
};

static void putback(lexer *l, char c) {
  if (c != 0 && c != EOF)
    --l->pos;
  l->putback = c;
}

static char skip_whitespaces(lexer *l) {
  char c;

  while (1) {
    c = next_char(l);
    switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
      break;

    case '/': {
      char next = next_char(l);
      if (next == '/') {
        // line comment
        while ((c = next_char(l)) != '\n' && c != EOF)
          ;
        break;
      } else if (next == '*') {
        // block comment
        char prev = 0;
        while (1) {
          c = next_char(l);
          if (c == EOF) {
            fprintf(stderr, "unterminated block comment on line %d\n", l->line);
            after_error();
          }
          if (prev == '*' && c == '/')
            break;
          prev = c;
        }
        break;
      } else {
        putback(l, next);
        return '/';
      }
    }
    default:
      return c;
    }
  }
}

// scans string literal, doesnt expect first '"', but will consume second
static string scan_string(lexer *l) {
  size_t i = 0;
  char c;

  while ((c = next_char(l)) != '"' && c != EOF) {
    if (i >= IDENT_BUF_LEN - 1) {
      fprintf(stderr, "string literal is too long, on line %d\n", l->line);
      after_error();
    }

    l->ident_buf[i++] = c;
  }

  if (c != '"') {
    fprintf(stderr, "unterminated string literal, on line %d\n", l->line);
    after_error();
  }

  l->ident_buf[i] = '\0';
  return new_string(l->ident_buf);
}

// checks if next char is given one, advances and returns 1 if it is,
// returns 0 otherwise
static char match_char(lexer *l, char c) {
  char to_check = next_char(l);
  if (to_check == c)
    return 1;
  putback(l, to_check);
  return 0;
}

// return pos of character c in string s, or -1 if not found
static int chrpos(char *s, int c) {
  char *p;

  p = strchr(s, c);
  return (p ? p - s : -1);
}

static int_literal_suffix convert_suff(const char *s, int line, int pos) {
  char u = false, l = false, ll = false;

  for (int i = 0; s[i]; i++) {
    if (s[i] == 'u' || s[i] == 'U') {
      if (u)
        goto invalid; // duplicate
      u = true;
    } else if (s[i] == 'l' || s[i] == 'L') {
      if (!l)
        l = s[i];
      else if (!ll) {
        if (l != s[i]) {
          goto invalid;
        }
        ll = s[i];
      } else
        goto invalid; // more than two L's
    }
  }

  if (ll && u)
    return INT_SUFF_ULL;
  if (ll)
    return INT_SUFF_LL;
  if (l && u)
    return INT_SUFF_UL;
  if (l)
    return INT_SUFF_L;
  if (u)
    return INT_SUFF_U;
  return INT_SUFF_NONE;

invalid:
  printf("invalid integer suffix '%s' on line %d, pos %d\n", s, line, pos);
  after_error();
  return INT_SUFF_NONE;
}

static void scan_int(lexer *l, char c, token *t) {
  uint64_t val = 0;
  int k = 0;

  // Convert each character into an int value
  while ((k = chrpos("0123456789", c)) >= 0) {
    val = val * 10 + k;
    c = next_char(l);
  }

  static char suffix_buf[4]; // max len is 3, ULL
  suffix_buf[0] = suffix_buf[1] = suffix_buf[2] = suffix_buf[3] = INT_SUFF_NONE;
  int n = 0;

  if (isalpha(c) && !((c == 'u' || c == 'U' || c == 'l' || c == 'L'))) {
    printf("invalid identifier on line %d, pos %d\n", l->line, l->pos);
    after_error();
  }

  while (c == 'u' || c == 'U' || c == 'l' || c == 'L') {
    if (n < 3)
      suffix_buf[n++] = c;
    c = next_char(l);
  }
  suffix_buf[n] = '\0';

  putback(l, c);
  t->v.int_lit.v = val;
  t->v.int_lit.suff = convert_suff(suffix_buf, l->line, l->pos);
}

static uint64_t scan_simple_int(lexer *l, char c) {
  uint64_t val = 0;
  int k = 0;

  // Convert each character into an int value
  while (1) {
    if ((k = chrpos("0123456789", c)) < 0)
      break;
    val = val * 10 + k;
    c = next_char(l);
  }

  putback(l, c);
  return val;
}

// reads preprocessor line directive in form like '# 5 "src/main.c" 2'
static char handle_preprocessor_directive(lexer *l) {
  char c;
  next_char(l); // skip ' '
  c = next_char(l);
  if (!isdigit(c)) {
    printf("invalid preprocessor directive, expected digit, found %c\n", c);
    after_error();
  }
  l->line = scan_simple_int(l, c);
  next_char(l); // skip ' '
  if (!match_char(l, '"')) {
    printf("invalid preprocessor directive, expected '\"', found %c\n", c);
    after_error();
  }

  // read file name
  l->f_name = scan_string(l);
  // TODO: not ideal, we are allocating new string
  // for same file name, but should be fine for now

  // skip till \n
  do {
    c = next_char(l);
  } while (c != '\n' && c != EOF);

  --l->line; // correct for '\n' which will happen in loop above

  return c;
}

static size_t scan_ident(lexer *l, char c) {
  size_t i = 0;

  while (isalpha(c) || isdigit(c) || c == '_') {
    if (IDENT_BUF_LEN - 1 == i) {
      fprintf(stderr, "identifier is too long, on line %d\n", l->line);
      after_error();
    } else if (i < IDENT_BUF_LEN - 1)
      l->ident_buf[i++] = c;
    c = next_char(l);
  }

  putback(l, c);
  l->ident_buf[i] = '\0';
  return i;
}

#define check_kw(kw, tok)                                                      \
  if (strcmp(l->ident_buf, kw) == 0)                                           \
  return tok

// returns keyword token type, or 0 if not keyword
int is_keyword(lexer *l) {
  switch (l->ident_buf[0]) {
  case 'v':
    check_kw("void", TOK_VOID);
    break;
  case 'c':
    check_kw("char", TOK_CHAR);
    else check_kw("continue", TOK_CONTINUE);
    else check_kw("case", TOK_CASE);
    break;
  case 'i':
    check_kw("int", TOK_INT);
    else check_kw("if", TOK_IF);
    break;
  case 'l':
    check_kw("long", TOK_LONG);
    break;
  case 'e':
    check_kw("else", TOK_ELSE);
    else check_kw("enum", TOK_ENUM);
    else check_kw("extern", TOK_EXTERN);
    break;
  case 'w':
    check_kw("while", TOK_WHILE);
    break;
  case 'f':
    check_kw("for", TOK_FOR);
    break;
  case 'r':
    check_kw("return", TOK_RETURN);
    break;
  case 's':
    check_kw("struct", TOK_STRUCT);
    else check_kw("switch", TOK_SWITCH);
    else check_kw("sizeof", TOK_SIZEOF);
    else check_kw("static", TOK_STATIC);
    break;
  case 'u':
    check_kw("union", TOK_UNION);
    break;
  case 't':
    check_kw("typedef", TOK_TYPEDEF);
    break;
  case 'b':
    check_kw("break", TOK_BREAK);
    break;
  case 'd':
    check_kw("default", TOK_DEFAULT);
    else check_kw("do", TOK_DO);
  case 'g':
    check_kw("goto", TOK_GOTO);
  }

  return 0;
}

#define double_tok(c, tok_long, tok)                                           \
  if (match_char(l, c))                                                        \
    t->token = tok_long;                                                       \
  else                                                                         \
    t->token = tok;                                                            \
  break;

#define triple_tok(c1, c2, tok_1, tok_2, tok)                                  \
  if (match_char(l, c1))                                                       \
    t->token = tok_1;                                                          \
  else if (match_char(l, c2))                                                  \
    t->token = tok_2;                                                          \
  else                                                                         \
    t->token = tok;                                                            \
  break;

void next(lexer *l, token *t) {
  char c = skip_whitespaces(l);
  t->pos.line = l->line;
  t->pos.filename = l->f_name;
  t->pos.start_pos = l->pos;

  switch (c) {
  case EOF:
    t->token = TOK_EOF;
    break;

  // Arithmetic
  case '+':
    triple_tok('=', '+', TOK_ASPLUS, TOK_DOUBLE_PLUS, TOK_PLUS);
  case '-':
    triple_tok('=', '-', TOK_ASMINUS, TOK_DOUBLE_MINUS, TOK_MINUS);
  case '*':
    double_tok('=', TOK_ASSTAR, TOK_STAR);
  case '/':
    double_tok('=', TOK_ASSLASH, TOK_SLASH);
  case '%':
    double_tok('=', TOK_ASPERCENT, TOK_MOD);

  // Bitwise
  case '&':
    triple_tok('=', '&', TOK_ASAMP, TOK_DOUBLE_AMP, TOK_AMPER);
  case '|':
    triple_tok('=', '|', TOK_ASPIPE, TOK_DOUBLE_PIPE, TOK_PIPE);
  case '^':
    double_tok('=', TOK_ASCARET, TOK_CARRET);
  case '~':
    t->token = TOK_TILDE;
    break;

  // Logical / comparison
  case '!':
    double_tok('=', TOK_NE, TOK_EXCL);
  case '=':
    double_tok('=', TOK_EQ, TOK_ASSIGN);
  case '<':
    if (match_char(l, '=')) {
      t->token = TOK_LE;
    } else if (match_char(l, '<')) {
      if (match_char(l, '='))
        t->token = TOK_ASLSHIFT;
      else
        t->token = TOK_LSHIFT;
    } else {
      t->token = TOK_LT;
    }
    break;
  case '>':
    if (match_char(l, '=')) {
      t->token = TOK_GE;
    } else if (match_char(l, '>')) {
      if (match_char(l, '='))
        t->token = TOK_ASRSHIFT;
      else
        t->token = TOK_RSHIFT;
    } else {
      t->token = TOK_GT;
    }
    break;

  // Ternary
  case '?':
    t->token = TOK_QUESTION;
    break;
  case ':':
    t->token = TOK_COLON;
    break;

  // Structural
  case ';':
    t->token = TOK_SEMI;
    break;
  case '{':
    t->token = TOK_LBRACE;
    break;
  case '}':
    t->token = TOK_RBRACE;
    break;
  case '(':
    t->token = TOK_LPAREN;
    break;
  case ')':
    t->token = TOK_RPAREN;
    break;
  case '[':
    t->token = TOK_LBRACKET;
    break;
  case ']':
    t->token = TOK_RBRACKET;
    break;
  case ',':
    t->token = TOK_COMMA;
    break;
  case '.':
    t->token = TOK_DOT;
    break;

  case '"':
    t->token = TOK_STRLIT;
    t->v.s_val = scan_string(l);
    break;

  default:
    if (isdigit(c)) {
      t->token = TOK_INTLIT;
      scan_int(l, c, t);
      break;
    } else if (isalpha(c) || c == '_') {
      scan_ident(l, c);

      if ((t->token = is_keyword(l)))
        break;

      t->token = TOK_IDENT;
      t->v.ident = new_string(l->ident_buf);
      break;
    }
    fprintf(stderr, "invalid character on line %d, pos %d", l->line, l->pos);
    after_error();
    break;
  }

  t->pos.end_pos = l->pos;
}
