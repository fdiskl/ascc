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

static uint64_t scan_int(lexer *l, char c) {
  uint64_t val = 0;
  int k = 0;

  // Convert each character into an int value
  while (1) {
    if (isalpha(c) || c == '_') {
      printf("invalid identifier starting with num on line %d", l->line);
      after_error();
    }

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
  l->line = scan_int(l, c);
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

size_t scan_ident(lexer *l, char c) {
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
  t->line = l->line;
  t->filename = l->f_name;
  t->start_pos = l->pos;

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
      t->v.int_val = scan_int(l, c);
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

  t->end_pos = l->pos;
}

const char *token_name(int token) {
  switch (token) {
  case TOK_EOF:
    return "EOF";

  // assignment operators
  case TOK_ASSIGN:
    return "=";
  case TOK_ASPLUS:
    return "+=";
  case TOK_ASMINUS:
    return "-=";
  case TOK_ASSTAR:
    return "*=";
  case TOK_ASSLASH:
    return "/=";
  case TOK_ASPERCENT:
    return "%=";
  case TOK_ASLSHIFT:
    return "<<=";
  case TOK_ASRSHIFT:
    return ">>=";
  case TOK_ASAMP:
    return "&=";
  case TOK_ASCARET:
    return "^=";
  case TOK_ASPIPE:
    return "|=";

  // logical
  case TOK_DOUBLE_PIPE:
    return "||";
  case TOK_DOUBLE_AMP:
    return "&&";
  case TOK_EXCL:
    return "!";

  // bitwise
  case TOK_PIPE:
    return "|";
  case TOK_CARRET:
    return "^";
  case TOK_AMPER:
    return "&";
  case TOK_TILDE:
    return "~";

  // equality and relational
  case TOK_EQ:
    return "==";
  case TOK_NE:
    return "!=";
  case TOK_LT:
    return "<";
  case TOK_GT:
    return ">";
  case TOK_LE:
    return "<=";
  case TOK_GE:
    return ">=";

  // shift
  case TOK_LSHIFT:
    return "<<";
  case TOK_RSHIFT:
    return ">>";

  // arithmetic
  case TOK_PLUS:
    return "+";
  case TOK_MINUS:
    return "-";
  case TOK_STAR:
    return "*";
  case TOK_SLASH:
    return "/";
  case TOK_MOD:
    return "%";
  case TOK_DOUBLE_PLUS:
    return "++";
  case TOK_DOUBLE_MINUS:
    return "--";

  // ternary
  case TOK_QUESTION:
    return "?";
  case TOK_COLON:
    return ":";

  // keywords
  case TOK_VOID:
    return "void";
  case TOK_CHAR:
    return "char";
  case TOK_INT:
    return "int";
  case TOK_LONG:
    return "long";
  case TOK_IF:
    return "if";
  case TOK_ELSE:
    return "else";
  case TOK_WHILE:
    return "while";
  case TOK_FOR:
    return "for";
  case TOK_RETURN:
    return "return";
  case TOK_STRUCT:
    return "struct";
  case TOK_UNION:
    return "union";
  case TOK_ENUM:
    return "enum";
  case TOK_TYPEDEF:
    return "typedef";
  case TOK_EXTERN:
    return "extern";
  case TOK_BREAK:
    return "break";
  case TOK_CONTINUE:
    return "continue";
  case TOK_SWITCH:
    return "switch";
  case TOK_CASE:
    return "case";
  case TOK_DEFAULT:
    return "default";
  case TOK_SIZEOF:
    return "sizeof";
  case TOK_STATIC:
    return "static";
  case TOK_CONST:
    return "const";

  // literals
  case TOK_INTLIT:
    return "<int literal>";
  case TOK_STRLIT:
    return "<string literal>";

  // structural
  case TOK_SEMI:
    return ";";
  case TOK_IDENT:
    return "<identifier>";
  case TOK_LBRACE:
    return "{";
  case TOK_RBRACE:
    return "}";
  case TOK_LPAREN:
    return "(";
  case TOK_RPAREN:
    return ")";
  case TOK_LBRACKET:
    return "[";
  case TOK_RBRACKET:
    return "]";
  case TOK_COMMA:
    return ",";
  case TOK_DOT:
    return ".";
  case TOK_ARROW:
    return "->";

  default:
    return "<unknown token>";
  }
}

void print_token(const token *t) {
  printf("[%s:%d:%3d:%3d] ", t->filename, t->line, t->start_pos, t->end_pos);

  const char *name = token_name(t->token);
  printf("Token: %s", name);

  switch (t->token) {
  case TOK_INTLIT:
    printf(" (%llu)", (unsigned long long)t->v.int_val);
    break;
  case TOK_STRLIT:
    printf(" (\"%s\")", t->v.s_val);
    break;
  case TOK_IDENT:
    printf(" (%s)", t->v.ident);
    break;
  default:
    break;
  }

  printf("\n");
}
