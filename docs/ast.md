# AST definition

```asdl
program = Program(decl*)
block_item = D(decl) | S(statement)
decl = var_decl | func_decl
var_decl = Var(identifier name, expr? init)
func_decl = Func(identifier name, block_item* body)
for_init = var_decl | expr
statement = Return(expr) | Block(block_item*) | Expr(expr)
          | Null | If(expr cond, statement then, statement? else)
          | Label(identifier, statement) | Goto(identifier)
          | While(expr cond, statement body) | DoWhile(statement body, expr cond)
          | For(for_init? init, expr? cond, expr? post, statement body)
          | Break | Continue
          | Case(constant_expr, statement) | Default(statement)
          | Switch(expr cond, statement body)
constant_expr = Constant(int) | Unary(unop, constant_expr)
              | Binary(binop constant_expr) | Conditional(constant_expr condition, expr then, expr else)
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

(For now `constant_expr = Constant(int)` because it requires expr eval which will be implemented after types are added)

# Formal grammar

```ebnf
<program> ::= { <decl> }
<block_item> ::= <decl> | <statement>
<decl> ::= <var_decl> | <func_decl>
<var_decl> ::= "int" <identifier> [ "=" <expr> ] ";"
<func_decl> ::= "int" <identifier> "(" "void" ")" "{" {<block_item>} "}"
<for-init> ::= <var_decl> | <func_decl>
<statement> ::= "return" <expr> | "{" {<block_item>} "}" | <expr> ";" | ";"
            | "if" "(" <expr> ")" <statement> [ "else" <statement> ]
            | <identifier> ":" <statement> | "goto" <identifier> ";"
            | "while" "(" <expr> ")" <statement>
            | "do" <statement> "while" "(" <expr> ")" ";"
            | "for" "(" [<for-init>] ";" [<expr>] ";" [<expr>] ) <statement>
            | "break" ";" | "continue" ";"
            | "case" <expr> ":" <statement> | "default" ":" <statement>
            | "switch" "(" <expr> ")" <statement>
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
