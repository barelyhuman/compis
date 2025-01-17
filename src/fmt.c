// SPDX-License-Identifier: Apache-2.0
#include "colib.h"
#include "compiler.h"
#include "abuf.h"

// TODO: rewrite this to use buf_t, like ast.c and cgen.c


const char* nodekind_fmt(nodekind_t kind) {
  switch (kind) {
    case EXPR_PARAM:
      return "parameter";
    case EXPR_LET:
      return "binding";
    case EXPR_VAR:
      return "variable";
    case EXPR_FUN:
      return "function";
    case EXPR_BLOCK:
      return "block";
    case EXPR_ID:
      return "identifier";
    case EXPR_PREFIXOP:
    case EXPR_POSTFIXOP:
    case EXPR_BINOP:
      return "operation";
    case EXPR_ASSIGN:
      return "assignment";
    case EXPR_DEREF:
      return "dereference";
    case EXPR_INTLIT:
    case EXPR_FLOATLIT:
    case EXPR_BOOLLIT:
      return "constant";
    case EXPR_MEMBER:
      return "member";
    case EXPR_FIELD:
      return "field";
    case TYPE_STRUCT:
      return "struct type";
    case TYPE_UNKNOWN:
      return "unknown type";
    case TYPE_UNRESOLVED:
      return "named type";
    case STMT_TYPEDEF:
      return "type definition";
    default:
      if (nodekind_istype(kind))
        return "type";
      if (nodekind_isexpr(kind))
        return "expression";
      return nodekind_name(kind);
  }
}


const char* op_fmt(op_t op) {
  switch ((enum op)op) {
  case OP_ALIAS:
  case OP_ARG:
  case OP_BORROW:
  case OP_BORROW_MUT:
  case OP_CALL:
  case OP_DROP:
  case OP_FCONST:
  case OP_FUN:
  case OP_ICONST:
  case OP_MOVE:
  case OP_NOOP:
  case OP_OCHECK:
  case OP_PHI:
  case OP_STORE:
  case OP_VAR:
  case OP_ZERO:
  case OP_CAST:
  case OP_GEP:
    return op_name(op);

  // unary
  case OP_INC:   return "++";
  case OP_DEC:   return "--";
  case OP_INV:   return "~";
  case OP_NOT:   return "!";
  case OP_DEREF: return "*";

  // binary, arithmetic
  case OP_ADD: return "+";
  case OP_SUB: return "-";
  case OP_MUL: return "*";
  case OP_DIV: return "/";
  case OP_MOD: return "%";

  // binary, bitwise
  case OP_AND: return "&";
  case OP_OR:  return "|";
  case OP_XOR: return "^";
  case OP_SHL: return "<<";
  case OP_SHR: return ">>";

  // binary, logical
  case OP_LAND: return "&&";
  case OP_LOR:  return "||";

  // binary, comparison
  case OP_EQ:   return "==";
  case OP_NEQ:  return "!=";
  case OP_LT:   return "<";
  case OP_GT:   return ">";
  case OP_LTEQ: return "<=";
  case OP_GTEQ: return ">=";

  // binary, assignment
  case OP_ASSIGN:     return "=";
  case OP_ADD_ASSIGN: return "+=";
  case OP_AND_ASSIGN: return "&=";
  case OP_DIV_ASSIGN: return "/=";
  case OP_MOD_ASSIGN: return "%=";
  case OP_MUL_ASSIGN: return "*=";
  case OP_OR_ASSIGN:  return "|=";
  case OP_SHL_ASSIGN: return "<<=";
  case OP_SHR_ASSIGN: return ">>=";
  case OP_SUB_ASSIGN: return "-=";
  case OP_XOR_ASSIGN: return "^=";
  }
  assertf(0,"bad op %u", op);
  return "?";
}


// static void fmtarray(abuf_t* s, const ptrarray_t* a, u32 depth, u32 maxdepth) {
//   for (usize i = 0; i < a->len; i++) {
//     abuf_c(s, ' ');
//     repr(s, a->v[i], indent, fl);
//   }
// }


static void startline(abuf_t* s, u32 indent) {
  if (s->len) abuf_c(s, '\n');
  abuf_fill(s, ' ', (usize)indent * 2);
}


static void fmt(abuf_t* s, const node_t* nullable n, u32 indent, u32 maxdepth);


static void local(abuf_t* s, const local_t* nullable n, u32 indent, u32 maxdepth) {
  abuf_str(s, n->name);
  abuf_c(s, ' ');
  fmt(s, (node_t*)n->type, indent, maxdepth);
  if (n->init && maxdepth > 1) {
    abuf_str(s, " = ");
    fmt(s, (node_t*)n->init, indent, maxdepth);
  }
}


static void funtype(abuf_t* s, const funtype_t* nullable n, u32 indent, u32 maxdepth) {
  assert(maxdepth > 0);
  abuf_c(s, '(');
  for (u32 i = 0; i < n->params.len; i++) {
    if (i) abuf_str(s, ", ");
    const local_t* param = n->params.v[i];
    abuf_str(s, param->name);
    if (i+1 == n->params.len || ((local_t*)n->params.v[i+1])->type != param->type) {
      abuf_c(s, ' ');
      fmt(s, (const node_t*)param->type, indent, maxdepth);
    }
  }
  abuf_str(s, ") ");
  fmt(s, (const node_t*)n->result, indent, maxdepth);
}


static void structtype(
  abuf_t* s, const structtype_t* nullable t, u32 indent, u32 maxdepth)
{
  if (t->name)
    abuf_str(s, t->name);
  if (maxdepth <= 1) {
    if (!t->name)
      abuf_str(s, "struct");
    return;
  }
  if (t->name)
    abuf_c(s, ' ');
  abuf_c(s, '{');
  if (t->fields.len > 0) {
    indent++;
    for (u32 i = 0; i < t->fields.len; i++) {
      startline(s, indent);
      const local_t* f = t->fields.v[i];
      abuf_str(s, f->name), abuf_c(s, ' ');
      fmt(s, (const node_t*)f->type, indent, maxdepth);
      if (f->init) {
        abuf_str(s, " = ");
        fmt(s, (const node_t*)f->init, indent, maxdepth);
      }
    }
    indent--;
    startline(s, indent);
  }
  abuf_c(s, '}');
}


static void fmt_nodelist(
  abuf_t* s, const ptrarray_t* nodes, const char* sep, u32 indent, u32 maxdepth)
{
  for (u32 i = 0; i < nodes->len; i++) {
    if (i) abuf_str(s, sep);
    fmt(s, nodes->v[i], indent, maxdepth);
  }
}


static void fmt(abuf_t* s, const node_t* nullable n, u32 indent, u32 maxdepth) {
  if (maxdepth == 0)
    return;
  if (!n)
    return abuf_str(s, "(NULL)");
  switch ((enum nodekind)n->kind) {

  case NODE_UNIT: {
    const ptrarray_t* a = &((unit_t*)n)->children;
    for (u32 i = 0; i < a->len; i++) {
      startline(s, indent);
      fmt(s, a->v[i], indent, maxdepth - 1);
    }
    break;
  }

  case STMT_TYPEDEF:
    abuf_fmt(s, "type ");
    fmt(s, (node_t*)&((typedef_t*)n)->type, indent, maxdepth);
    break;

  case EXPR_VAR:
  case EXPR_LET:
    abuf_str(s, n->kind == EXPR_VAR ? "var " : "let ");
    FALLTHROUGH;
  case EXPR_PARAM:
  case EXPR_FIELD:
    return local(s, (const local_t*)n, indent, maxdepth);

  case EXPR_FUN: {
    fun_t* fn = (fun_t*)n;
    funtype_t* ft = (funtype_t*)fn->type;
    abuf_fmt(s, "fun %s(", fn->name);
    fmt_nodelist(s, &ft->params, ", ", indent, maxdepth);
    abuf_str(s, ") ");
    fmt(s, (node_t*)ft->result, indent, maxdepth);
    if (fn->body) {
      abuf_c(s, ' ');
      fmt(s, (node_t*)fn->body, indent, maxdepth);
    }
    break;
  }

  case EXPR_BLOCK: {
    abuf_c(s, '{');
    const ptrarray_t* a = &((block_t*)n)->children;
    if (a->len > 0) {
      if (maxdepth <= 1) {
        abuf_str(s, "...");
      } else {
        indent++;
        for (u32 i = 0; i < a->len; i++) {
          startline(s, indent);
          fmt(s, a->v[i], indent, maxdepth - 1);
        }
        indent--;
        startline(s, indent);
      }
    }
    abuf_c(s, '}');
    break;
  }

  case EXPR_CALL: {
    const call_t* call = (const call_t*)n;
    fmt(s, (const node_t*)call->recv, indent, maxdepth);
    abuf_c(s, '(');
    fmt_nodelist(s, &call->args, ", ", indent, maxdepth);
    abuf_c(s, ')');
    break;
  }

  case EXPR_TYPECONS: {
    const typecons_t* tc = (const typecons_t*)n;
    fmt(s, (const node_t*)tc->type, indent, maxdepth);
    abuf_c(s, '(');
    fmt(s, (const node_t*)tc->expr, indent, maxdepth);
    abuf_c(s, ')');
    break;
  }

  case EXPR_MEMBER:
    fmt(s, (node_t*)((const member_t*)n)->recv, indent, maxdepth);
    abuf_c(s, '.');
    abuf_str(s, ((const member_t*)n)->name);
    break;

  case EXPR_IF:
    abuf_str(s, "if ");
    fmt(s, (node_t*)((const ifexpr_t*)n)->cond, indent, maxdepth);
    abuf_c(s, ' ');
    fmt(s, (node_t*)((const ifexpr_t*)n)->thenb, indent, maxdepth);
    if (((const ifexpr_t*)n)->elseb) {
      abuf_str(s, " else ");
      fmt(s, (node_t*)((const ifexpr_t*)n)->elseb, indent, maxdepth);
    }
    break;

  case EXPR_FOR:
    if (maxdepth <= 1) {
      abuf_str(s, "for");
    } else {
      forexpr_t* e = (forexpr_t*)n;
      abuf_str(s, "for ");
      if (e->start || e->end) {
        if (e->start)
          fmt(s, (node_t*)e->start, indent, maxdepth - 1);
        abuf_str(s, "; ");
        fmt(s, (node_t*)e->cond, indent, maxdepth - 1);
        abuf_str(s, "; ");
        if (e->end)
          fmt(s, (node_t*)e->start, indent, maxdepth - 1);
      } else {
        fmt(s, (node_t*)e->cond, indent, maxdepth - 1);
      }
      abuf_c(s, ' ');
      fmt(s, (node_t*)e->body, indent, maxdepth - 1);
    }
    break;

  case EXPR_ID:
    abuf_str(s, ((idexpr_t*)n)->name);
    break;

  case EXPR_RETURN:
    abuf_str(s, "return");
    if (((const retexpr_t*)n)->value) {
      abuf_c(s, ' ');
      fmt(s, (node_t*)((const retexpr_t*)n)->value, indent, maxdepth);
    }
    break;

  case EXPR_DEREF:
  case EXPR_PREFIXOP:
    switch ( ((unaryop_t*)n)->op ) {
    case OP_INC: abuf_str(s, "++"); break;
    case OP_DEC: abuf_str(s, "--"); break;
    case OP_INV: abuf_str(s, "~"); break;
    case OP_NOT: abuf_str(s, "!"); break;
    }
    fmt(s, (node_t*)((unaryop_t*)n)->expr, indent, maxdepth);
    break;

  case EXPR_POSTFIXOP:
    fmt(s, (node_t*)((unaryop_t*)n)->expr, indent, maxdepth);
    abuf_str(s, op_fmt(((unaryop_t*)n)->op));
    break;

  case EXPR_ASSIGN:
  case EXPR_BINOP:
    fmt(s, (node_t*)((binop_t*)n)->left, indent, maxdepth - 1);
    abuf_c(s, ' ');
    abuf_str(s, op_fmt(((binop_t*)n)->op));
    abuf_c(s, ' ');
    fmt(s, (node_t*)((binop_t*)n)->right, indent, maxdepth - 1);
    break;

  case EXPR_BOOLLIT:
    abuf_str(s, ((const intlit_t*)n)->intval ? "true" : "false");
    break;

  case EXPR_INTLIT: {
    const intlit_t* lit = (const intlit_t*)n;
    if (lit->type && type_isunsigned(lit->type)) {
      abuf_str(s, "0x");
      abuf_u64(s, lit->intval, 16);
    } else {
      abuf_u64(s, lit->intval, 10);
    }
    break;
  }

  case EXPR_FLOATLIT:
    abuf_f64(s, ((const floatlit_t*)n)->f64val, -1);
    break;

  case EXPR_STRLIT: {
    const strlit_t* str = (strlit_t*)n;
    abuf_c(s, '"');
    abuf_repr(s, str->bytes, str->len);
    abuf_c(s, '"');
    break;
  }

  case EXPR_ARRAYLIT:
    abuf_c(s, '[');
    if (maxdepth <= 1) {
      abuf_str(s, "...");
    } else {
      fmt_nodelist(s, &((arraylit_t*)n)->values, ", ", indent, maxdepth);
    }
    abuf_c(s, ']');
    break;

  case TYPE_VOID: abuf_str(s, "void"); break;
  case TYPE_BOOL: abuf_str(s, "bool"); break;
  case TYPE_I8:   abuf_str(s, "i8"); break;
  case TYPE_I16:  abuf_str(s, "i16"); break;
  case TYPE_I32:  abuf_str(s, "i32"); break;
  case TYPE_I64:  abuf_str(s, "i64"); break;
  case TYPE_INT:  abuf_str(s, "int"); break;
  case TYPE_U8:   abuf_str(s, "u8"); break;
  case TYPE_U16:  abuf_str(s, "u16"); break;
  case TYPE_U32:  abuf_str(s, "u32"); break;
  case TYPE_U64:  abuf_str(s, "u64"); break;
  case TYPE_UINT: abuf_str(s, "uint"); break;
  case TYPE_F32:  abuf_str(s, "f32"); break;
  case TYPE_F64:  abuf_str(s, "f64"); break;

  case TYPE_STRUCT:
    return structtype(s, (const structtype_t*)n, indent, maxdepth);
  case TYPE_FUN:
    abuf_str(s, "fun");
    return funtype(s, (const funtype_t*)n, indent, maxdepth);
  case TYPE_ARRAY: {
    arraytype_t* t = (arraytype_t*)n;
    abuf_c(s, '[');
    fmt(s, (node_t*)t->elem, indent, maxdepth);
    if (t->len > 0)
      abuf_fmt(s, " %llu", t->len);
    abuf_c(s, ']');
    break;
  }
  case TYPE_SLICE:
  case TYPE_MUTSLICE: {
    slicetype_t* t = (slicetype_t*)n;
    abuf_str(s, "&[");
    fmt(s, (node_t*)t->elem, indent, maxdepth);
    abuf_c(s, ']');
    break;
  }
  case TYPE_PTR: {
    const ptrtype_t* pt = (const ptrtype_t*)n;
    abuf_c(s, '*');
    fmt(s, (node_t*)pt->elem, indent, maxdepth);
    break;
  }
  case TYPE_REF:
  case TYPE_MUTREF: {
    const reftype_t* pt = (const reftype_t*)n;
    abuf_str(s, n->kind == TYPE_MUTREF ? "mut&" : "&");
    fmt(s, (node_t*)pt->elem, indent, maxdepth);
    break;
  }
  case TYPE_OPTIONAL: {
    abuf_c(s, '?');
    fmt(s, (node_t*)((const opttype_t*)n)->elem, indent, maxdepth);
    break;
  }

  case TYPE_ALIAS: {
    const aliastype_t* at = (const aliastype_t*)n;
    abuf_str(s, at->name);
    if (maxdepth > 1) {
      abuf_c(s, ' ');
      fmt(s, (node_t*)at->elem, indent, maxdepth);
    }
    break;
  }

  case TYPE_UNKNOWN:
    abuf_str(s, "unknown");
    break;

  case TYPE_UNRESOLVED:
    abuf_str(s, ((unresolvedtype_t*)n)->name);
    break;

  case NODE_BAD:
    abuf_fmt(s, "/*NODE_BAD*/");
    break;

  case NODE_COMMENT:
    abuf_fmt(s, "/*comment*/");
    break;

  case NODEKIND_COUNT:
    assertf(0, "unexpected node %s", nodekind_name(n->kind));
  }
}


err_t node_fmt(buf_t* buf, const node_t* n, u32 maxdepth) {
  usize needavail = 64;
  maxdepth = MAX(maxdepth, 1);
  for (;;) {
    buf_reserve(buf, needavail);
    abuf_t s = abuf_make(buf->chars + buf->len, buf->cap - buf->len);
    fmt(&s, n, 0, maxdepth);
    usize len = abuf_terminate(&s);
    if (len < needavail) {
      buf->len += len;
      break;
    }
    needavail = len + 1;
  }
  return 0;
}
