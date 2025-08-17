# AST definition

```
program = Program(decl*)
decl = Var(identifier name) | Func(identifier name, statement body)
statement = Return(expr) | Block(statement*)
expr = Constant(int) | Unary(unop, expr) | Binary(binop, expr)
unop = Complement | Negate
binop = Add | Sub | Mul | Div | Mod
```

# Formal grammar

```ebnf
<program> ::= { <decl> }
<decl> ::= <var_decl> | <func_decl>
<var_decl> ::= "int" <identifier>
<func_decl> ::= "int" <identifier> "(" "void" ")" "{" {<statement>} "}"
<statement> ::= "return" <expr>
<expr> ::= <factor> | <expr> <binop> <expr>
<factor> ::= <int> | <unop> <factor> | "(" <expr> ")"
<binop> ::= "+" | "-" | "*" | "/" | "%"
<unop> ::= "-" | "~"
<identifier> ::= ? ident token ?
<int> ::= ? int literal token ?
```
