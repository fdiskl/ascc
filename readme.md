# Another

# Self compiling

# C

# Compiler

(ascc)

---

### compiler stages:

0. _preprocess (by gcc)_

1. lex
2. parse
   2.1 resolve idents
3. typecheck
4. gen tac
5. gen x86 asm
   5.1 fix pseudo operands
   5.2 fix instructios
6. emit asm

7. _assemble (by gcc)_
8. _link (by gcc)_
