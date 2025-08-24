# AST definition

```
program = Program(decl*)
block_item = D(decl) | S(statement)
decl = Var(identifier name, expr? init) | Func(identifier name, block_item* body)
statement = Return(expr) | Block(block_item*) | Expr(expr) | Null
expr = Constant(int) | Var(identifier) | Unary(unop, expr) | Binary(binop, expr) | Assignment(exp, exp)
unop = Complement | Negate | Not
binop = Add | Sub | Mul | Div | Mod
      | BitwiseAnd | BitwiseOr | BitwiseXor | Lshift | Rshift
      | And | Or | Eq | NotEq | Lt | Gt | LtEq| GtEq
```

# Formal grammar

```ebnf
<program> ::= { <decl> }
<block_item> ::= <decl> | <statement>
<decl> ::= <var_decl> | <func_decl>
<var_decl> ::= "int" <identifier> [ "=" <expr> ] ";"
<func_decl> ::= "int" <identifier> "(" "void" ")" "{" {<block_item>} "}"
<statement> ::= "return" <expr> | "{" {<block_item>} "}" | <expr> ";" | ";"
<expr> ::= <factor> | <expr> <binop> <expr>
<factor> ::= <int> | <identifier> | <unop> <factor> | "(" <expr> ")"
<binop> ::= "+" | "-" | "*" | "/" | "%"
        | "&" | "|" | "^" | "<<" | ">>"
        | "&&" | "||" | "==" | "!=" | "<" | ">" | "<=" | ">=" | "="
<unop> ::= "-" | "~" | "!"
<identifier> ::= ? ident token ?
<int> ::= ? int literal token ?
```
