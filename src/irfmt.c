// SPDX-License-Identifier: Apache-2.0
#include "c0lib.h"
#include "compiler.h"


typedef struct {
  buf_t out;
  bool  ok;
} fmtctx_t;


#define MEMFAIL()            (ctx->ok = false, ((void)0))
#define CHAR(ch)             buf_push(&ctx->out, (ch))
#define PRINT(cstr)          buf_print(&ctx->out, (cstr))
#define PRINTF(fmt, args...) buf_printf(&ctx->out, (fmt), ##args)
#define FILL(byte, len)      buf_fill(&ctx->out, (byte), (len))
#define TABULATE(start, dstcol)   \
  FILL(' ', (usize)(MAX(1, (int)(dstcol) - (int)(ctx->out.len - (start)))));

#define COMMENT_COL  32


static void val(fmtctx_t* ctx, const irval_t* v) {
  u32 start = ctx->out.len + 1;
  PRINTF("\n    v%-2u ", v->id);

  u32 tstart = ctx->out.len;
  node_fmt(&ctx->out, (node_t*)v->type, 0);
  PRINTF("%*s = %-*s",
    MAX(0, 4 - (int)(ctx->out.len - tstart)), "",
    6, op_name(v->op) + 3/*"OP_"*/);

  for (u32 i = 0; i < v->argc; i++)
    PRINTF(" v%-2u", v->argv[i]->id);

  switch (v->op) {
  case OP_ARG:
    PRINTF(" %u", v->aux.i32val);
    break;
  case OP_ICONST:
    PRINTF(" 0x%llu", v->aux.i64val);
    break;
  case OP_FCONST:
    PRINTF(" %g", v->aux.f64val);
    break;
  }

  TABULATE(start, COMMENT_COL);
  PRINTF("# [%u]", v->nuse);
  if (v->comment && *v->comment)
    CHAR(' '), PRINT(v->comment);
  if (v->loc.line) {
    TABULATE(start, COMMENT_COL + 10);
    if (v->loc.input) {
      PRINTF(" %s:%u:%u", v->loc.input->name, v->loc.line, v->loc.col);
    } else {
      PRINTF(" %u:%u", v->loc.line, v->loc.col);
    }
  }
}


static void block(fmtctx_t* ctx, const irblock_t* b) {
  u32 start = ctx->out.len + 1;
  PRINTF("\n  b%u:", b->id);

  if (b->preds[1]) {
    PRINTF(" <- b%u b%u", assertnotnull(b->preds[0])->id, b->preds[1]->id);
  } else if (b->preds[0]) {
    PRINTF(" <- b%u", b->preds[0]->id);
  }

  if (b->comment) {
    TABULATE(start, COMMENT_COL);
    PRINTF("# %s", b->comment);
  }

  for (u32 i = 0; i < b->values.len; i++)
    val(ctx, b->values.v[i]);

  switch (b->kind) {
    case IR_BLOCK_CONT: {
      if (b->succs[0]) {
        PRINTF("\n  cont -> b%u", b->succs[0]->id);
      } else {
        PRINT("\n  cont -> ?");
      }
      break;
    }
    case IR_BLOCK_FIRST:
    case IR_BLOCK_IF: {
      assert(b->succs[0]); // thenb
      assert(b->succs[1]); // elseb
      assertf(b->control, "missing control value");
      PRINTF("\n  %s v%u -> b%u b%u",
        (b->kind == IR_BLOCK_IF) ? "if" : "first",
        b->control->id, b->succs[0]->id, b->succs[1]->id);
      break;
    }
    case IR_BLOCK_RET: {
      if (b->control) {
        PRINTF("\n  ret v%u", b->control->id);
      } else {
        PRINT("\n  ret");
      }
      break;
    }
  }
}


static void fun(fmtctx_t* ctx, const irfun_t* f) {
  PRINTF("\nfun %s(", f->name);
  if (f->ast) {
    for (u32 i = 0; i < f->ast->params.len; i++) {
      const local_t* param = f->ast->params.v[i];
      if (i) PRINT(", ");
      node_fmt(&ctx->out, (node_t*)param, 0);
    }
    PRINT(") ");
    const type_t* restype = ((funtype_t*)f->ast->type)->result;
    node_fmt(&ctx->out, (node_t*)restype, 0);
    CHAR(' ');
    PRINT("{");
  } else {
    PRINT(") {");
  }
  for (u32 i = 0; i < f->blocks.len; i++)
    block(ctx, f->blocks.v[i]);
  PRINT("\n}");
}


static void unit(fmtctx_t* ctx, const irunit_t* u) {
  for (u32 i = 0; i < u->functions.len; i++)
    fun(ctx, u->functions.v[i]);
}


bool irfmt(buf_t* out, const irunit_t* u) {
  fmtctx_t ctx = { .ok = true, .out = *out };
  unit(&ctx, u);
  *out = ctx.out;
  return ctx.ok && buf_nullterm(out);
}
