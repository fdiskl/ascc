# AST definition

```asdl
program = Program(decl*)
block_item = D(decl) | S(statement)
decl = Var(identifier name, expr? init) | Func(identifier name, block_item* body)
statement = Return(expr) | Block(block_item*) | Expr(expr) | Null | If(exp cond, statement then, statement? else`)
expr = Constant(int) | Var(identifier) | Unary(unop, expr)
     | Binary(binop, expr) | Assignment(assignment_op, exp, exp)
     | Conditional(expr condition, expr then, expr else)
unop = Complement | Negate | Not
     | PrefixInc | PrefixDec | PostifxInc | PostfixDec
binop = Add | Sub | Mul | Div | Mod
      | BitwiseAnd | BitwiseOr | BitwiseXor | Lshift | Rshift
      | And | Or | Eq | NotEq | Lt | Gt | LtEq| GtEq
assignment_op =  Assign | AddAssign | SubAssign | MulAssign
               | DivAssign | ModAssign
               | BitwiseAndAssgin | BitwiseOrAssign
               | XorAssign | LshiftAssign | RshiftAssign
```

# Formal grammar

```ebnf
<program> ::= { <decl> }
<block_item> ::= <decl> | <statement>
<decl> ::= <var_decl> | <func_decl>
<var_decl> ::= "int" <identifier> [ "=" <expr> ] ";"
<func_decl> ::= "int" <identifier> "(" "void" ")" "{" {<block_item>} "}"
<statement> ::= "return" <expr> | "{" {<block_item>} "}" | <expr> ";" | ";"
            | "if" "(" <expr> ")" <statement> [ "else" <statement> ]
<expr> ::= <factor> | <expr> <binop> <expr> | <expr> "?" <expr> ":" <expr>
<factor> ::= <int> | <identifier> | <unary> | "(" <expr> ")"
<unary> ::= <prefix-unary> | <postfix-unary>
<prefix-unary> ::= <prefix-unop> <factor>
<postfix-unary> ::= <factor> <postfix-unop>
<binop> ::= "+" | "-" | "*" | "/" | "%"
        | "&" | "|" | "^" | "<<" | ">>"
        | "&&" | "||" | "==" | "!=" | "<" | ">" | "<=" | ">="
        | "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" | "<<=" ">>="
<prefix-unop> ::= "-" | "~" | "!" | "++" | "--"
<postfix-unop> ::= "++" | "--"
<identifier> ::= ? ident token ?
<int> ::= ? int literal token ?
```
