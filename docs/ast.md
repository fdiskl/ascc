# AST definition

```asdl
program = Program(decl*)
block_item = D(decl) | S(statement)
decl = var_decl | func_decl
var_decl = Var(identifier name, expr? init, type var_type, storage_class?)
func_decl = Func(identifier name, identifier* params, block_item* body, type func_type, storage_class?)
storage_class = Static | Extern
for_init = V(var_decl) | E(expr)
statement = Return(expr) | Block(block_item*) | Expr(expr)
          | Null | If(expr cond, statement then, statement? else)
          | Label(identifier, statement) | Goto(identifier)
          | While(expr cond, statement body) | DoWhile(statement body, expr cond)
          | For(for_init? init, expr? cond, expr? post, statement body)
          | Break | Continue
          | Case(constant_expr, statement) | Default(statement)
          | Switch(expr cond, statement body)
          | FuncCall(identifier name, expr* args)
constant_expr = Constant(const)
              | Unary(unop, constant_expr)
              | Binary(binop constant_expr)
              | Conditional(constant_expr condition, constant_expr then, constant_expr else)
              | Cast(type target, constant_expr e)
expr = Constant(int) | Var(identifier) | Unary(unop, expr)
     | Binary(binop, expr) | Assignment(assignment_op, exp, exp)
     | Conditional(expr condition, expr then, expr else)
     | Cast(type target, expr)
unop = Complement | Negate | Not
     | PrefixInc | PrefixDec | PostifxInc | PostfixDec
binop = Add | Sub | Mul | Div | Mod
      | BitwiseAnd | BitwiseOr | BitwiseXor | Lshift | Rshift
      | And | Or | Eq | NotEq | Lt | Gt | LtEq| GtEq
assignment_op =  Assign | AddAssign | SubAssign | MulAssign
               | DivAssign | ModAssign
               | BitwiseAndAssgin | BitwiseOrAssign
               | XorAssign | LshiftAssign | RshiftAssign
const = ConstInt(int) | ConstLong(int) | ConstUInt(int) | ConstULong(int) | ConstDouble(float)
type = Int | Long | UInt | ULong | FuncType(type *params, type ret) | Double
```

(For now compiler actually uses `constant_expr = const` because it requires expr eval which will be implemented after types are added)

# Formal grammar

```ebnf
<program> ::= { <decl> }
<block_item> ::= <decl> | <statement>
<decl> ::= <var_decl> | <func_decl>
<var_decl> ::= {<specifier>}+ <identifier> [ "=" <expr> ] ";"
<func_decl> ::= {<specifier>}+ <identifier> "(" <params> ")" ( ("{" {<block_item>} "}") | ";" )
<specifier> ::= <type_specifier> | "extern" | "static"
<params> ::= "void" | { <type_specifier> }+ <identifier> {"," { <type_specifier> }+ <identifier>}
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
<factor> ::= <const> | "(" { <type_specifier> }+ ")" <factor>
         | <identifier> | <unary> | "(" <expr> ")"
         | <identifier> "(" [ <expr> {"," <expr>} ] ")"
<unary> ::= <prefix-unary> | <postfix-unary>
<prefix-unary> ::= <prefix-unop> <factor>
<postfix-unary> ::= <factor> <postfix-unop>
<binop> ::= "+" | "-" | "*" | "/" | "%"
        | "&" | "|" | "^" | "<<" | ">>"
        | "&&" | "||" | "==" | "!=" | "<" | ">" | "<=" | ">="
        | "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" | "<<=" ">>="
<type_specifier> ::= "int" | "long" | "unsigned" | "signed" | "double"
<prefix-unop> ::= "-" | "~" | "!" | "++" | "--"
<postfix-unop> ::= "++" | "--"
<identifier> ::= ? ident token ?
<const> ::= <int> | <long> | <uint> | <ulong>
<int> ::= ? int literal token ?
<long> ::= ? long literal token ?
<uint> ::= ? unsigned int literal token ?
<ulong> ::= ? unsigned long literal token ?
<double> ::= ? double floatinhg point number literal token ?
```
