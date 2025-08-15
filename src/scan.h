#ifndef _ASCC_SCAN_H
#define _ASCC_SCAN_H

#include "strings.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef struct _token token;
typedef struct _lexer lexer;
typedef struct _tok_pos tok_pos;

#define IDENT_BUF_LEN 1023

struct _tok_pos {
  int line;
  int start_pos;
  int end_pos;
  string filename;
};

struct _lexer {
  int line;      // curr line
  int pos;       // curr position
  FILE *f;       // curr file reader
  string f_name; // curr file name
  char putback;  // putback character, 0 if none

  char ident_buf[IDENT_BUF_LEN]; // tmp buffer to store idents
};

void init_lexer(lexer *l, FILE *f);
void next(lexer *l, token *t);
void print_token(const token *t);
const char *token_name(int token);

struct _token {
  int token; // token type
  union {
    uint64_t int_val; // for TOK_INTLIT
    string s_val;     // for TOK_STRLIT
    string ident;     // for TOK_IDENT
  } v;
  tok_pos pos;
};

enum {
  TOK_EOF,

  // assignment operators
  TOK_ASSIGN,    // =
  TOK_ASPLUS,    // +=
  TOK_ASMINUS,   // -=
  TOK_ASSTAR,    // *=
  TOK_ASSLASH,   // /=
  TOK_ASPERCENT, // %=
  TOK_ASLSHIFT,  // <<=
  TOK_ASRSHIFT,  // >>=
  TOK_ASAMP,     // &=
  TOK_ASCARET,   // ^=
  TOK_ASPIPE,    // |=

  // logical operators
  TOK_DOUBLE_PIPE, // ||
  TOK_DOUBLE_AMP,  // &&
  TOK_EXCL,        // !

  // bitwise operators
  TOK_PIPE,   // |
  TOK_CARRET, // ^
  TOK_AMPER,  // &
  TOK_TILDE,  // ~

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
  TOK_PLUS,         // +
  TOK_MINUS,        // -
  TOK_STAR,         // *
  TOK_SLASH,        // /
  TOK_MOD,          // %
  TOK_DOUBLE_PLUS,  // ++
  TOK_DOUBLE_MINUS, // --

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
  TOK_CONST,

  // literals
  TOK_INTLIT,
  TOK_STRLIT,

  // structural
  TOK_SEMI,     // ;
  TOK_IDENT,    // <foo>
  TOK_LBRACE,   // {
  TOK_RBRACE,   // }
  TOK_LPAREN,   // (
  TOK_RPAREN,   // )
  TOK_LBRACKET, // [
  TOK_RBRACKET, // ]
  TOK_COMMA,    // ,
  TOK_DOT,      // .
  TOK_ARROW,    // ->
};

#endif
