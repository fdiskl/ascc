#include "scan.h"
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
            exit(1);
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
      exit(1);
    }

    l->ident_buf[i++] = c;
  }

  if (c != '"') {
    fprintf(stderr, "unterminated string literal, on line %d\n", l->line);
    exit(1);
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

static int_literal_suffix convert_int_suff(const char *s, int line, int pos) {
  bool u = false;
  char l = 0; // 'l' or 'L'
  char ll = 0;

  int first_l = -1;
  int last_l = -1;

  for (int i = 0; s[i]; i++) {
    char c = s[i];

    if (c == 'u' || c == 'U') {
      if (u)
        goto invalid;
      u = true;
    } else if (c == 'l' || c == 'L') {
      if (!l)
        l = c;
      else if (!ll) {
        if (l != c)
          goto invalid;
        ll = c;
      } else
        goto invalid; // >2 Ls

      if (first_l == -1)
        first_l = i;
      last_l = i;
    } else {
      goto invalid;
    }
  }

  if (first_l != -1) {
    for (int i = first_l; i <= last_l; i++) {
      if (s[i] != l)
        goto invalid;
    }
  }

  if (ll && u) {
    printf("integer suffix ULL is not supported (%d:%d)\n", line, pos);
    exit(1);
    return INT_SUFF_ULL;
  }
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
  exit(1);
  return INT_SUFF_NONE;
}

static float_literal_suffix convert_float_suff(const char *s, int line,
                                               int pos) {
  int ret;
  if (s[0]) {
    char c = s[0];
    switch (c) {
    case 'l':
    case 'L':
      ret = FLOAT_SUFF_L;
      break;
    case 'f':
    case 'F':
      ret = FLOAT_SUFF_F;
      break;
    }
  } else {
    return FLOAT_SUFF_NONE;
  }

  printf("float suffixes are not supported (%d:%d)\n", line, pos);
  exit(1);
  return ret;
}

static void scan_const(lexer *l, char c, token *t) {
  uint64_t ival = 0;
  long double fval = 0.0;
  int saw_digit = 0;

  while (isdigit(c)) {
    ival = ival * 10 + (c - '0');
    fval = fval * 10.0 + (c - '0');
    saw_digit = 1;
    c = next_char(l);
  }

  int is_float = 0;

  if (c == '.') {
    is_float = 1;
    c = next_char(l);

    if (!isdigit(c))
      putback(l, c);

    if (c == '_') {
      printf("malformed constant ('.' followed by '_') at line %d, pos %d\n",
             l->line, l->pos);
      exit(1);
    }
    if (!isdigit(c) && !saw_digit) {
      printf("malformed constant at line %d, pos %d\n", l->line, l->pos);
      exit(1);
    }

    double frac = 0.0;
    double base = 1.0;

    while (isdigit(c)) {
      saw_digit = 1;
      frac = frac * 10.0 + (c - '0');
      base *= 10.0;
      c = next_char(l);
    }

    fval += frac / base;
  }

  if (!saw_digit) {
    printf("invalid numeric literal at line %d, pos %d\n", l->line, l->pos);
    exit(1);
  }

  // exponent
  if (c == 'e' || c == 'E') {
    is_float = 1;
    c = next_char(l);

    int exp = 0;
    int sign = 1;

    if (c == '+' || c == '-') {
      if (c == '-')
        sign = -1;
      c = next_char(l);
    }

    if (!isdigit(c)) {
      printf("invalid exponent on line %d, pos %d\n", l->line, l->pos);
      exit(1);
    }

    while (isdigit(c)) {
      exp = exp * 10 + (c - '0');
      c = next_char(l);
    }

    if (c == '.') {
      printf("malformed exponent at line %d, pos %d\n", l->line, l->pos);
      exit(1);
    }

    long double pow10 = 1.0;
    for (int i = 0; i < exp; i++)
      pow10 *= 10.0;

    if (sign < 0)
      fval /= pow10;
    else
      fval *= pow10;
  }

  // suff
  static char suff[4]; // max len is ULL
  suff[0] = suff[1] = suff[2] = suff[3] = '\0';
  int n = 0;

  while (c == 'u' || c == 'U' || c == 'l' || c == 'L' || c == 'f' || c == 'F') {

    if (n < 3)
      suff[n++] = c;

    // float suffix forces float
    if (c == 'f' || c == 'F')
      is_float = 1;

    else if ((c == 'l' || c == 'L') && is_float)
      is_float = 1; // redundant, but shows logic

    c = next_char(l);
  }

  if (isalpha(c)) {
    printf("invalid numeric literal on line %d, pos %d\n", l->line, l->pos);
    exit(1);
  }

  putback(l, c);

  if (is_float) {
    t->token = TOK_FLOATLIT;
    t->v.float_lit.v = fval;
    t->v.float_lit.suff = convert_float_suff(suff, l->line, l->pos);
  } else {
    t->token = TOK_INTLIT;
    t->v.int_lit.v = ival;
    t->v.int_lit.suff = convert_int_suff(suff, l->line, l->pos);
  }
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
    exit(1);
  }
  l->line = scan_simple_int(l, c);
  next_char(l); // skip ' '
  if (!match_char(l, '"')) {
    printf("invalid preprocessor directive, expected '\"', found %c\n", c);
    exit(1);
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
      exit(1);
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
    else check_kw("signed", TOK_SIGNED);
    break;
  case 'u':
    check_kw("union", TOK_UNION);
    else check_kw("unsigned", TOK_UNSIGNED);
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
    else check_kw("double", TOK_DOUBLE);
    break;
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
  case '.': {
    char next_c = next_char(l);

    if (isdigit(next_c)) {
      putback(l, c);
      scan_const(l, c, t);
    } else {
      t->token = TOK_DOT;
    }

    break;
  }

  case '"':
    t->token = TOK_STRLIT;
    t->v.s_val = scan_string(l);
    break;

  default:
    if (isdigit(c)) {
      scan_const(l, c, t);
      break;
    } else if (isalpha(c) || c == '_') {
      scan_ident(l, c);

      if ((t->token = is_keyword(l)))
        break;

      t->token = TOK_IDENT;
      t->v.ident = new_string(l->ident_buf);
      break;
    }
    fprintf(stderr, "invalid character on line %d, pos %d\n", l->line, l->pos);
    exit(1);
    break;
  }

  t->pos.end_pos = l->pos;
}
