#include "scan.h"

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
