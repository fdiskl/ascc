
#include "x86.h"
#include "arena.h"
#include "common.h"
#include "parser.h"
#include "table.h"
#include "tac.h"
#include "type.h"
#include "typecheck.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void init_x86_asm_gen(x86_asm_gen *ag, sym_table *st) {
  NEW_ARENA(ag->instr_arena, x86_instr);
  NEW_ARENA(ag->top_level_arena, x86_top_level);
  ag->st = st;
}

x86_instr *alloc_x86_instr(x86_asm_gen *ag, int op) {
  x86_instr *res = ARENA_ALLOC_OBJ(ag->instr_arena, x86_instr);
  res->next = NULL;
  res->prev = NULL;
  res->op = op;
  res->origin = NULL;
  return res;
}

static x86_instr *insert_x86_instr(x86_asm_gen *ag, int op, taci *origin) {
  x86_instr *i = alloc_x86_instr(ag, op);
  if (ag->head == NULL)
    ag->head = i;
  else {
    i->prev = ag->tail;
    ag->tail->next = i;
  }
  ag->tail = i;

  i->origin = origin;

  return i;
}

static x86_top_level *alloc_x86_func(x86_asm_gen *ag, string name) {
  x86_top_level *res = ARENA_ALLOC_OBJ(ag->top_level_arena, x86_top_level);
  res->next = NULL;
  res->is_func = true;
  res->v.f.name = name;
  res->v.f.first = NULL;
  return res;
}

static x86_top_level *alloc_x86_static_var(x86_asm_gen *ag, string name) {
  x86_top_level *res = ARENA_ALLOC_OBJ(ag->top_level_arena, x86_top_level);
  res->next = NULL;
  res->is_func = false;
  res->v.v.name = name;
  return res;
}

static x86_op new_x86_imm(uint64_t v) {
  x86_op op;
  op.t = X86_OP_IMM;
  op.v.imm = v;
  return op;
}

static x86_asm_type get_x86_asm_type_from_type(type *t) {
  switch (t->t) {
  case TYPE_INT:
    return X86_LONGWORD;
  case TYPE_LONG:
    return X86_QUADWORD;
  case TYPE_UINT:
    return X86_LONGWORD;
  case TYPE_ULONG:
    return X86_QUADWORD;
  case TYPE_FN:
    UNREACHABLE();
    break;
  }
}

// NOTE: when using get_x86_asm_type, its doesnt really matter which tacv is
// given, bc types already should be equal
static x86_asm_type get_x86_asm_type(x86_asm_gen *ag, tacv v) {
  switch (v.t) {
  case TACV_CONST:
    switch (v.v.iconst.t) {
    case CONST_INT:
      return X86_LONGWORD;
    case CONST_LONG:
      return X86_QUADWORD;
    case CONST_UINT:
      return X86_LONGWORD;
    case CONST_ULONG:
      return X86_QUADWORD;
    }
  case TACV_VAR: {
    syme *e = ht_get(ag->st->t, v.v.var);
    assert(e);
    return get_x86_asm_type_from_type(e->t);
  }
  }
}

static x86_op new_x86_reg(x86_reg reg) {
  x86_op op;
  op.t = X86_OP_REG;
  op.v.reg = reg;
  return op;
}

static x86_op new_x86_pseudo(string name) {
  x86_op op;
  op.t = X86_OP_PSEUDO;
  op.v.pseudo = name;
  return op;
}

static x86_op operand_from_tac_val(tacv v) {
  switch (v.t) {
  case TACV_CONST:
    return new_x86_imm(v.v.iconst.v);
  case TACV_VAR:
    return new_x86_pseudo(v.v.var);
  }
  UNREACHABLE();
}

static void gen_asm_from_ret_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  insert_x86_instr(ag, X86_RET, i);

  mov->v.binary.dst = new_x86_reg(X86_AX);
  mov->v.binary.src = operand_from_tac_val(i->v.s.src1);
  mov->v.binary.type = get_x86_asm_type(ag, i->v.s.src1);
}

static void gen_asm_from_unary_instr(x86_asm_gen *ag, taci *i) {
  int op;
  switch (i->op) {
  case TAC_NEGATE:
    op = X86_NEG;
    break;
  case TAC_COMPLEMENT:
    op = X86_NOT;
    break;
  default:
    UNREACHABLE();
  }

  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);

  mov->v.binary.dst = operand_from_tac_val(i->dst);
  mov->v.binary.src = operand_from_tac_val(i->v.s.src1);
  mov->v.binary.type = get_x86_asm_type(ag, i->dst);

  x86_instr *u = insert_x86_instr(ag, op, i);
  u->v.unary.src = operand_from_tac_val(i->dst);
  u->v.unary.type = get_x86_asm_type(ag, i->dst);
}

static void gen_asm_from_binary_instr(x86_asm_gen *ag, taci *i) {
  if (i->op == TAC_DIV || i->op == TAC_MOD) {
    assert(i->dst.t == TACV_VAR);
    syme *e = ht_get(ag->st->t, i->dst.v.var);
    assert(e);
    bool is_signed = type_signed(e->t);

    x86_instr *mov1 = insert_x86_instr(ag, X86_MOV, i);
    x86_instr *ext;
    x86_instr *idiv = insert_x86_instr(ag, is_signed ? X86_IDIV : X86_DIV, i);
    x86_instr *mov2 = insert_x86_instr(ag, X86_MOV, i);

    if (is_signed) {
      ext = insert_x86_instr(ag, X86_CDQ, i);
      ext->v.cdq.type = get_x86_asm_type(ag, i->dst);
    } else {
      ext = insert_x86_instr(ag, X86_MOV, i);
      ext->v.binary.src = new_x86_imm(0);
      ext->v.binary.dst = new_x86_reg(i->op == TAC_DIV ? X86_DX : X86_AX);
      ext->v.binary.type = get_x86_asm_type(ag, i->dst);
    }

    mov1->v.binary.dst = new_x86_reg(X86_AX);
    mov1->v.binary.src = operand_from_tac_val(i->v.s.src1);
    mov1->v.binary.type = get_x86_asm_type(ag, i->v.s.src1);

    idiv->v.unary.src = operand_from_tac_val(i->v.s.src2);
    idiv->v.unary.type = get_x86_asm_type(ag, i->v.s.src2);

    mov2->v.binary.src = new_x86_reg(i->op == TAC_DIV ? X86_AX : X86_DX);
    mov2->v.binary.dst = operand_from_tac_val(i->dst);
    mov2->v.binary.type = get_x86_asm_type(ag, i->dst);

    return;
  }

  int op;
  switch (i->op) {
  case TAC_ADD:
    op = X86_ADD;
    break;
  case TAC_SUB:
    op = X86_SUB;
    break;
  case TAC_MUL:
    op = X86_MULT;
    break;
  case TAC_AND:
    op = X86_AND;
    break;
  case TAC_OR:
    op = X86_OR;
    break;
  case TAC_XOR:
    op = X86_XOR;
    break;
  case TAC_LSHIFT:
    op = X86_SHL;
    break;
  case TAC_RSHIFT:
    op = X86_SAR;
    break;
  default:
    UNREACHABLE();
  }

  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  x86_instr *bini = insert_x86_instr(ag, op, i);

  mov->v.binary.dst = operand_from_tac_val(i->dst);
  mov->v.binary.src = operand_from_tac_val(i->v.s.src1);
  mov->v.binary.type = get_x86_asm_type(ag, i->dst);

  bini->v.binary.dst = mov->v.binary.dst;
  bini->v.binary.src = operand_from_tac_val(i->v.s.src2);
  bini->v.binary.type = get_x86_asm_type(ag, i->dst);
}

static void gen_asm_from_not_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *cmp = insert_x86_instr(ag, X86_CMP, i);
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  x86_instr *sete = insert_x86_instr(ag, X86_SETCC, i);

  cmp->v.binary.dst = operand_from_tac_val(i->v.s.src1);
  cmp->v.binary.src = new_x86_imm(0);
  cmp->v.binary.type = get_x86_asm_type(ag, i->v.s.src1);

  mov->v.binary.src = new_x86_imm(0);
  sete->v.setcc.op = mov->v.binary.dst = operand_from_tac_val(i->dst);

  mov->v.binary.type = get_x86_asm_type(ag, i->dst);

  sete->v.setcc.cc = CC_E;
}

static void gen_asm_from_cpy_instr(x86_asm_gen *ag, taci *i) {
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  mov->v.binary.src = operand_from_tac_val(i->v.s.src1);
  mov->v.binary.dst = operand_from_tac_val(i->dst);
  mov->v.binary.type = get_x86_asm_type(ag, i->dst);
}

static void gen_asm_from_comparing_instr(x86_asm_gen *ag, taci *i) {
  assert(i->dst.t == TACV_VAR);

  syme *e = ht_get(ag->st->t, i->dst.v.var);
  assert(e);

  bool is_signed = type_signed(e->t);

  x86_instr *cmp = insert_x86_instr(ag, X86_CMP, i);
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  x86_instr *setcc = insert_x86_instr(ag, X86_SETCC, i);

  cmp->v.binary.dst = operand_from_tac_val(i->v.s.src1);
  cmp->v.binary.src = operand_from_tac_val(i->v.s.src2);
  cmp->v.binary.type = get_x86_asm_type(ag, i->v.s.src1);

  mov->v.binary.src = new_x86_imm(0);

  setcc->v.setcc.op = mov->v.binary.dst = operand_from_tac_val(i->dst);

  mov->v.binary.type = get_x86_asm_type(ag, i->dst);

  int cc;
  switch (i->op) {
  case TAC_EQ:
    cc = CC_E;
    break;
  case TAC_NE:
    cc = CC_NE;
    break;
  case TAC_LT:
    cc = is_signed ? CC_L : CC_B;
    break;
  case TAC_LE:
    cc = is_signed ? CC_LE : CC_BE;
    break;
  case TAC_GT:
    cc = is_signed ? CC_G : CC_A;
    break;
  case TAC_GE:
    cc = is_signed ? CC_GE : CC_AE;
    break;
  default:
    UNREACHABLE();
  }

  setcc->v.setcc.cc = cc;
}

static void gen_asm_from_jump_instr(x86_asm_gen *ag, taci *i) {
  if (i->op == TAC_JMP) {
    x86_instr *res = insert_x86_instr(ag, X86_JMP, i);
    res->v.label = i->label_idx;
    return;
  }

  x86_instr *cmp = insert_x86_instr(ag, X86_CMP, i);
  x86_instr *jcc = insert_x86_instr(ag, X86_JMPCC, i);

  cmp->v.binary.src =
      i->op == TAC_JE ? operand_from_tac_val(i->v.s.src2) : new_x86_imm(0);
  cmp->v.binary.dst = operand_from_tac_val(i->v.s.src1);
  cmp->v.binary.type = get_x86_asm_type(ag, i->v.s.src1);

  jcc->v.jmpcc.cc = i->op == TAC_JZ || i->op == TAC_JE ? CC_E : CC_NE;
  jcc->v.jmpcc.label_idx = i->label_idx;
}

static void gen_asm_from_label_instr(x86_asm_gen *ag, taci *i) {
  insert_x86_instr(ag, X86_LABEL, i)->v.label = i->label_idx;
}

static void gen_asm_from_inc_dec(x86_asm_gen *ag, taci *i) {
  x86_instr *instr =
      insert_x86_instr(ag, i->op == TAC_INC ? X86_INC : X86_DEC, i);

  instr->v.unary.src = operand_from_tac_val(i->v.s.src1);
  instr->v.unary.type = get_x86_asm_type(ag, i->v.s.src1);
}

static void gen_asm_from_assign(x86_asm_gen *ag, taci *i) {
  if (i->op == TAC_ASDIV || i->op == TAC_ASMOD) {
    x86_instr *mov1 = insert_x86_instr(ag, X86_MOV, i);
    x86_instr *cdq = insert_x86_instr(ag, X86_CDQ, i);
    x86_instr *idiv = insert_x86_instr(ag, X86_IDIV, i);
    x86_instr *mov2 = insert_x86_instr(ag, X86_MOV, i);

    cdq->v.cdq.type = get_x86_asm_type(ag, i->dst);

    mov1->v.binary.dst = new_x86_reg(X86_AX);
    mov2->v.binary.dst = mov1->v.binary.src = operand_from_tac_val(i->dst);
    mov2->v.binary.type = mov1->v.binary.type = get_x86_asm_type(ag, i->dst);
    mov2->v.binary.src = new_x86_reg(i->op == TAC_ASDIV ? X86_AX : X86_DX);

    idiv->v.unary.src = operand_from_tac_val(i->v.s.src1);

    mov1->v.binary.type = get_x86_asm_type(ag, i->v.s.src1);
    mov2->v.binary.type = get_x86_asm_type(ag, i->dst);
    idiv->v.unary.type = get_x86_asm_type(ag, i->v.s.src2);
    return;
  }

  int op;

  switch (i->op) {
  case TAC_ASADD:
    op = X86_ADD;
    break;
  case TAC_ASSUB:
    op = X86_SUB;
    break;
  case TAC_ASMUL:
    op = X86_MULT;
    break;
  case TAC_ASAND:
    op = X86_AND;
    break;
  case TAC_ASOR:
    op = X86_OR;
    break;
  case TAC_ASXOR:
    op = X86_XOR;
    break;
  case TAC_ASLSHIFT:
    op = X86_SHL;
    break;
  case TAC_ASRSHIFT:
    op = X86_SAR;
    break;
  default:
    UNREACHABLE();
  }

  x86_instr *instr = insert_x86_instr(ag, op, i);

  instr->v.binary.dst = operand_from_tac_val(i->dst);
  instr->v.binary.src = operand_from_tac_val(i->v.s.src1);
  instr->v.binary.type = get_x86_asm_type(ag, i->dst);
}

static x86_reg arg_regs[6] = {
    X86_DI, X86_SI, X86_DX, X86_CX, X86_R8, X86_R9,
};

static void gen_asm_from_call(x86_asm_gen *ag, taci *i) {
  int padding = 0;
  if (i->v.call.args_len % 2 == 1) {
    x86_instr *alloc_instr = insert_x86_instr(ag, X86_SUB, i);
    alloc_instr->v.binary.dst = new_x86_reg(X86_SP);
    alloc_instr->v.binary.src = new_x86_imm(padding = 8);
    alloc_instr->v.binary.type = X86_QUADWORD;
  }

  for (int j = 0;
       j < sizeof(arg_regs) / sizeof(x86_reg) && j < i->v.call.args_len; ++j) {
    x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
    mov->v.binary.dst = new_x86_reg(arg_regs[j]);
    mov->v.binary.src = operand_from_tac_val(i->v.call.args[j]);
    mov->v.binary.type = get_x86_asm_type(ag, i->v.call.args[j]);
  }

  int stack_args = 0;
  for (int j = i->v.call.args_len - 1; j > 5; --j) {
    ++stack_args;
    x86_op op = operand_from_tac_val(i->v.call.args[j]);

    x86_asm_type asm_type = get_x86_asm_type(ag, i->v.call.args[j]);
    if (op.t == X86_OP_REG || op.t == X86_OP_IMM || asm_type == X86_QUADWORD) {
      x86_instr *push = insert_x86_instr(ag, X86_PUSH, i);
      push->v.unary.src = op;
      push->v.unary.type = X86_QUADWORD;
    } else {
      // work around bc pushing 4 byte value by using 8 byte pushq will cause
      // problems so we move 4 byte value into eax and then push rax.
      // quadwords dont need this workaround so they are handled in if before
      x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
      mov->v.binary.dst = new_x86_reg(X86_AX);
      mov->v.binary.src = op;
      mov->v.binary.type = X86_LONGWORD;
      x86_instr *push = insert_x86_instr(ag, X86_PUSH, i);
      push->v.unary.src = new_x86_reg(X86_AX);
      push->v.unary.type = X86_QUADWORD;
    }
  }

  x86_instr *call = insert_x86_instr(ag, X86_CALL, i);
  call->v.call.str_label = i->v.call.name;
  padding += 8 * stack_args;
  if (padding != 0) {
    x86_instr *dealloc_instr = insert_x86_instr(ag, X86_ADD, i);
    dealloc_instr->v.binary.dst = new_x86_reg(X86_SP);
    dealloc_instr->v.binary.src = new_x86_imm(padding);
    dealloc_instr->v.binary.type = X86_QUADWORD;
  }

  x86_op dst = operand_from_tac_val(i->dst);
  x86_instr *mov = insert_x86_instr(ag, X86_MOV, i);
  mov->v.binary.dst = dst;
  mov->v.binary.src = new_x86_reg(X86_AX);
  syme *e = ht_get(ag->st->t, i->v.call.name);
  assert(e);
  assert(e->t->t == TYPE_FN);
  mov->v.binary.type = get_x86_asm_type_from_type(e->t->v.fntype.return_type);
}

static void gen_asm_from_sextend(x86_asm_gen *ag, taci *i) {
  x86_instr *res = insert_x86_instr(ag, X86_MOVSX, i);
  res->v.binary.dst = operand_from_tac_val(i->dst);
  res->v.binary.src = operand_from_tac_val(i->v.s.src1);
}

static void gen_asm_from_zextend(x86_asm_gen *ag, taci *i) {
  x86_instr *res = insert_x86_instr(ag, X86_MOVZEXT, i);
  res->v.binary.dst = operand_from_tac_val(i->dst);
  res->v.binary.src = operand_from_tac_val(i->v.s.src1);
}

static void gen_asm_from_truncate(x86_asm_gen *ag, taci *i) {
  x86_instr *res = insert_x86_instr(ag, X86_MOV, i);
  res->v.binary.dst = operand_from_tac_val(i->dst);
  res->v.binary.src = operand_from_tac_val(i->v.s.src1);
  res->v.binary.type = X86_LONGWORD;
}

static void gen_asm_from_instr(x86_asm_gen *ag, taci *i) {
  switch (i->op) {
  case TAC_RET:
    gen_asm_from_ret_instr(ag, i);
    break;
  case TAC_NEGATE:
  case TAC_COMPLEMENT:
    gen_asm_from_unary_instr(ag, i);
    break;
  case TAC_ADD:
  case TAC_SUB:
  case TAC_MUL:
  case TAC_DIV:
  case TAC_MOD:
  case TAC_AND:
  case TAC_OR:
  case TAC_XOR:
  case TAC_LSHIFT:
  case TAC_RSHIFT:
    gen_asm_from_binary_instr(ag, i);
    break;
  case TAC_NOT:
    gen_asm_from_not_instr(ag, i);
    break;
  case TAC_CPY:
    gen_asm_from_cpy_instr(ag, i);
    break;
  case TAC_EQ:
  case TAC_NE:
  case TAC_LT:
  case TAC_LE:
  case TAC_GT:
  case TAC_GE:
    gen_asm_from_comparing_instr(ag, i);
    break;
  case TAC_JMP:
  case TAC_JZ:
  case TAC_JNZ:
  case TAC_JE:
    gen_asm_from_jump_instr(ag, i);
    break;
  case TAC_LABEL:
    gen_asm_from_label_instr(ag, i);
    break;
  case TAC_INC:
  case TAC_DEC:
    gen_asm_from_inc_dec(ag, i);
    break;
  case TAC_ASADD:
  case TAC_ASSUB:
  case TAC_ASMUL:
  case TAC_ASDIV:
  case TAC_ASMOD:
  case TAC_ASAND:
  case TAC_ASOR:
  case TAC_ASXOR:
  case TAC_ASLSHIFT:
  case TAC_ASRSHIFT:
    gen_asm_from_assign(ag, i);
    break;
  case TAC_CALL:
    gen_asm_from_call(ag, i);
    break;
  case TAC_SIGN_EXTEND:
    gen_asm_from_sextend(ag, i);
    break;
  case TAC_TRUNCATE:
    gen_asm_from_truncate(ag, i);
    break;
  case TAC_ZERO_EXTEND:
    gen_asm_from_zextend(ag, i);
    break;
  }
}

static x86_top_level *gen_asm_from_func(x86_asm_gen *ag, tacf *f) {
  x86_top_level *func = alloc_x86_func(ag, f->name);
  func->v.f.global = f->global;
  ag->head = NULL;
  ag->tail = NULL;

  syme *fn_e = ht_get(ag->st->t, f->name);
  assert(fn_e);
  type *fn_type = fn_e->t;

  for (int i = 0; i < f->params_len && i < sizeof(arg_regs) / sizeof(x86_reg);
       ++i) {
    x86_instr *mov = insert_x86_instr(ag, X86_MOV, NULL);
    x86_op op;
    op.t = X86_OP_PSEUDO;
    op.v.pseudo = f->params[i];
    mov->v.binary.dst = op;
    mov->v.binary.src = new_x86_reg(arg_regs[i]);
    mov->v.binary.type =
        get_x86_asm_type_from_type(fn_type->v.fntype.params[i]); // !!!
  }

  for (int i = sizeof(arg_regs) / sizeof(x86_reg); i < f->params_len; ++i) {
    x86_instr *mov = insert_x86_instr(ag, X86_MOV, NULL);
    x86_op op;
    op.t = X86_OP_PSEUDO;
    op.v.pseudo = f->params[i];
    x86_op op2;
    op2.t = X86_OP_STACK;
    op2.v.stack_offset = (i - 5) * -8 - 8;
    mov->v.binary.dst = op;
    mov->v.binary.src = op2;
    mov->v.binary.type =
        get_x86_asm_type_from_type(fn_type->v.fntype.params[i]);
  }

  for (taci *i = f->firsti; i != NULL; i = i->next)
    gen_asm_from_instr(ag, i);

  func->v.f.first = ag->head;
  return func;
}

static int x86_alignment(inital_init_t t) {
  switch (t) {
  case INITIAL_INT:
  case INITIAL_UINT:
    return 4;
  case INITIAL_LONG:
  case INITIAL_ULONG:
    return 8;
  }
}

x86_top_level *gen_asm_from_static_var(x86_asm_gen *ag, tac_static_var *sv) {
  x86_top_level *res = alloc_x86_static_var(ag, sv->name);
  res->v.v.global = sv->global;
  res->v.v.init = sv->init;
  res->v.v.alignment = x86_alignment(sv->init.t);
  return res;
}

static void convert_to_be_syme(be_syme *e, syme *olde) {
  switch (olde->t->t) {
  case TYPE_INT:
  case TYPE_LONG:
  case TYPE_UINT:
  case TYPE_ULONG:
    e->t = BE_SYME_OBJ;
    e->v.obj.type = get_x86_asm_type_from_type(olde->t);
    e->v.obj.is_static = olde->a.t == ATTR_STATIC;
    break;
  case TYPE_FN:
    e->t = BE_SYME_FN;
    assert(olde->a.t == ATTR_FUNC);
    e->v.fn.defined = olde->a.v.f.defined;
    break;
  }
}

static void convert_symtable(arena *be_syme_arena, ht *be_st, sym_table *st) {
  ht *fe_st = st->t;

  hti it = ht_iterator(fe_st);
  while (ht_next(&it)) {
    be_syme *be_entry = ARENA_ALLOC_OBJ(be_syme_arena, be_syme);
    convert_to_be_syme(be_entry, (syme *)it.value);
    ht_set(be_st, it.key, be_entry);
  }
}

static const char *asm_type_name(x86_asm_type t) {
  switch (t) {
  case X86_LONGWORD:
    return "longw";
  case X86_QUADWORD:
    return "quadw";
  case X86_BYTE:
    return "byte";
    break;
  }
}

static void print_be_entry(be_syme *e) {
  switch (e->t) {
  case BE_SYME_OBJ:
    printf("obj (%s, static: %s)", asm_type_name(e->v.obj.type),
           e->v.obj.is_static ? "true" : "false");
    break;
  case BE_SYME_FN:
    printf("fn (defined: %s)", e->v.fn.defined ? "true" : "false");
    break;
  }
}

void emit_be_st(ht *be_st) {
  hti it = ht_iterator(be_st);
  while (ht_next(&it)) {
    printf("%s: ", it.key);
    print_be_entry(it.value);
    printf("\n");
  }
}

x86_program gen_asm(tac_program *prog, sym_table *st) {
  x86_program res;
  x86_asm_gen ag;

  init_x86_asm_gen(&ag, st);

  arena *be_syme_arena;
  NEW_ARENA(be_syme_arena, be_syme);

  ht *be_st = ht_create();

  res.be_syme_arena = be_syme_arena;
  res.be_st = be_st;

  convert_symtable(be_syme_arena, be_st, st);

  res.top_level_arena = ag.top_level_arena;
  res.instr_arena = ag.instr_arena;

  x86_top_level *head = NULL;
  x86_top_level *tail = NULL;
  for (tac_top_level *tl = prog->first; tl != NULL; tl = tl->next) {
    x86_top_level *res;
    if (tl->is_func) {
      res = gen_asm_from_func(&ag, &tl->v.f);

// 2 step fix
#ifndef ASM_DONT_FIX_PSEUDO
      x86_instr *alloc_instr = alloc_x86_instr(&ag, X86_SUB);

      alloc_instr->next = res->v.f.first;
      res->v.f.first = alloc_instr;
      alloc_instr->next->prev = alloc_instr;

      int bytes_to_alloc = fix_pseudo_for_func(&ag, &res->v.f, be_st);

      alloc_instr->v.binary.dst = new_x86_reg(X86_SP);
      alloc_instr->v.binary.src = new_x86_imm(bytes_to_alloc);
      alloc_instr->v.binary.type = X86_QUADWORD;

#endif

#ifndef ASM_DONT_FIX_INSTRUCTIONS
      fix_instructions_for_func(&ag, &res->v.f);
#endif
    } else {
      res = gen_asm_from_static_var(&ag, &tl->v.v);
    }

    res->next = NULL;
    if (head == NULL)
      head = res;
    else
      tail->next = res;
    tail = res;
  }

  res.first = head;

  return res;
}

void free_x86_program(x86_program *p) {
  destroy_arena(p->instr_arena);
  destroy_arena(p->top_level_arena);
  destroy_arena(p->be_syme_arena);
  ht_destroy(p->be_st);
}
