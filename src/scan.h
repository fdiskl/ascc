#ifndef _ASCC_SCAN_H
#define _ASCC_SCAN_H

#include "string.h"
#include <stdio.h>
typedef struct _token token;
typedef struct _lexer lexer;

struct _lexer {
  int line;
  int pos;
  FILE *f;
};

void init_lexer(lexer *l, FILE *f);
void next(lexer *l);

struct _token {
  int token; // token type
  union {
    int int_val;  // for TOK_INTLIT
    string s_val; // for TOK_STRLIT
  } v;
  int start_pos;
  int len;
  int line;
  string filename;
};

enum {
  TOK_EOF,

  // assignment operators
  TOK_ASSIGN,   // =
  TOK_ASPLUS,   // +=
  TOK_ASMINUS,  // -=
  TOK_ASSTAR,   // *=
  TOK_ASSLASH,  // /=
  TOK_ASMOD,    // %=
  TOK_ASLSHIFT, // <<=
  TOK_ASRSHIFT, // >>=
  TOK_ASAND,    // &=
  TOK_ASXOR,    // ^=
  TOK_ASOR,     // |=

  // logical operators
  TOK_LOGOR,  // ||
  TOK_LOGAND, // &&
  TOK_LOGNOT, // !

  // bitwise operators
  TOK_OR,     // |
  TOK_XOR,    // ^
  TOK_AMPER,  // &
  TOK_INVERT, // ~

  // equality and relational
  TOK_EQ, // ==
  TOK_NE, // !=
  TOK_LT, // <
  TOK_GT, // >
  TOK_LE, // <=
  TOK_GE, // >=

  // shift operators
  TOK_LSHIFT, // <<
  TOK_RSHIFT, // >>

  // arithmetic operators
  TOK_PLUS,  // +
  TOK_MINUS, // -
  TOK_STAR,  // *
  TOK_SLASH, // /
  TOK_MOD,   // %
  TOK_INC,   // ++
  TOK_DEC,   // --

  // ternary operator
  TOK_QUESTION, // ?
  TOK_COLON,    // :

  // keywords
  TOK_VOID,
  TOK_CHAR,
  TOK_INT,
  TOK_LONG,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_FOR,
  TOK_RETURN,
  TOK_STRUCT,
  TOK_UNION,
  TOK_ENUM,
  TOK_TYPEDEF,
  TOK_EXTERN,
  TOK_BREAK,
  TOK_CONTINUE,
  TOK_SWITCH,
  TOK_CASE,
  TOK_DEFAULT,
  TOK_SIZEOF,
  TOK_STATIC,

  // literals
  TOK_INTLIT,
  TOK_STRLIT,

  // structural
  TOK_SEMI,
  TOK_IDENT,
  TOK_LBRACE,
  TOK_RBRACE,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_LBRACKET,
  TOK_RBRACKET,
  TOK_COMMA,
  TOK_DOT,
  TOK_ARROW,
};

#endif
