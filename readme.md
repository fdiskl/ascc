# Another Self-hosting C Compiler

(ascc)

---

# Compiler stages:

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

---

# Implementation defined behaviors

## Converting long to int

Paragraph 3, section 6.3.1.3 of C99 standard _(ISO/IEC 9899:TC3)_ specifies

> the new type is signed and the value cannot be represented in it; either the result is implementation-defined or an implementation-defined signal is raised.

### Implementation

ascc will handle conversion in same way as GCC ([gcc docs 4.5](https://gcc.gnu.org/onlinedocs/gcc/Integers-implementation.html)) -

> The result of, or the signal raised by, converting an integer to a signed integer type when the value cannot be represented in an object of that type (C90 6.2.1.2, C99 and C11 6.3.1.3, C23 6.3.2.3).
>
> For conversion to a type of width N, the value is reduced modulo 2^N to be within range of the type; no signal is raised."

#### Example

Let's say we want to convert 2^31 (by 1 bigger then maximum int value) from long to int.
Then we will subtruct 2^32 from it: `2^31 - 2^32 = - 2^31`.
In practise upper 4 bytes of value will be dropped, if long can be represented by int - result won't change, if can't it has the result of reducing it's modulo by 2^32.
