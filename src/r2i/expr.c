#include "r2i/internal.h"

#include "sema/common.h"

static void r2i_print_escaped (R2i *r, unsigned char c, char quote)
{
	switch (c)
	{
	case '\\': r2i_printf(r, "\\\\"); return;
	case '\n': r2i_printf(r, "\\n"); return;
	case '\t': r2i_printf(r, "\\t"); return;
	case '\r': r2i_printf(r, "\\r"); return;
	case '\0': r2i_printf(r, "\\0"); return;
	case '\'': r2i_printf(r, quote == '\'' ? "\\'" : "'"); return;
	case '"': r2i_printf(r, quote == '"' ? "\\\"" : "\""); return;
	default:
		if (c < 32 || c == 127) r2i_printf(r, "\\x%02x", c);
		else r2i_printf(r, "%c", c);
		return;
	}
}

static void r2i_print_string (R2i *r, const char *str, uint16_t len)
{
	r2i_printf(r, "\"");
	for (uint16_t i = 0; i < len; i++)
		r2i_print_escaped(r, (unsigned char)str[i], '"');
	r2i_printf(r, "\"");
}

static void r2i_print_char (R2i *r, char c)
{
	r2i_printf(r, "'");
	r2i_print_escaped(r, (unsigned char)c, '\'');
	r2i_printf(r, "'");
}

static void r2i_print_fn_name (R2i *r, uint32_t sym)
{
	Symbol *s = r->ir->symtab->symbols + sym;

	if (s->ir_id < r->ir->fn_count)
		r2i_printf(r, "%s", r->ir->fns[s->ir_id].sym);
}

static void r2i_print_global_name (R2i *r, uint32_t sym)
{
	Symbol *s = r->ir->symtab->symbols + sym;

	if (s->ir_id < r->ir->global_count)
		r2i_printf(r, "%s", r->ir->globals[s->ir_id].sym);
}

static int r2i_print_literal (R2i *r, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_LIT_i64: case NODE_LIT_u64:
		if (types[n->data_type].sign)
			r2i_printf(r, "%lld", (long long)n->i64);
		else r2i_printf(r, "%llu", (unsigned long long)n->u64);
		return 1;
	case NODE_LIT_CHAR: r2i_print_char(r, n->chr); return 1;
	case NODE_LIT_STR: r2i_print_string(r, n->str, n->len); return 1;
	case NODE_LIT_NULL: r2i_printf(r, "NULL"); return 1;
	default: return 0;
	}
}

static int r2i_print_symbol (R2i *r, ASTNode *n)
{
	if (n->type == NODE_ID) {
		if (n->sym == NO_SYMBOL) return 0;
		Symbol *s = r->ir->symtab->symbols + n->sym;

		if (s->kind == SYMBOL_FN) {
			if (s->ir_id >= r->ir->fn_count) return 0;
			r2i_print_fn_name(r, n->sym);
			return 1;
		}
		if (s->kind == SYMBOL_VAR) {
			if (s->ir_id >= r->ir->global_count) return 0;
			r2i_print_global_name(r, n->sym);
			return 1;
		}
		return 0;
	}
	if (n->type != NODE_ADDR) return 0;
	uint32_t child = n->child;

	if (child == NO_NODE) return 0;
	uint32_t sym = r->ir->ast->nodes[child].sym;

	if (sym == NO_SYMBOL) return 0;
	Symbol *s = r->ir->symtab->symbols + sym;

	if (s->ir_id >= r->ir->global_count) return 0;
	r2i_printf(r, "$");
	r2i_print_global_name(r, sym);
	return 1;
}

static void r2i_print_cast (R2i *r, ASTNode *n)
{
	ASTNode *child = r->ir->ast->nodes + n->child;

	r2i_print_expr(r, n->child);
	if (child->type == NODE_STRUCT_INIT || child->type == NODE_ARRAY_INIT
			|| child->data_type != n->data_type) {
		r2i_printf(r, " as ");
		r2i_print_type(r, n->data_type);
	}
}

static void r2i_print_init_list (R2i *r, uint32_t first)
{
	r2i_printf(r, "{");
	for (uint32_t item = first; item != NO_NODE;
			item = r->ir->ast->nodes[item].next_bro) {
		if (item != first) r2i_printf(r, ", ");
		r2i_print_expr(r, item);
	}
	r2i_printf(r, "}");
}

static void r2i_print_index_init (R2i *r, ASTNode *n)
{
	uint32_t dim = n->child;

	r2i_printf(r, "[");
	for (uint32_t k = 0; k < n->sym; k++) {
		if (k) r2i_printf(r, "][");
		r2i_print_expr(r, dim);
		dim = r->ir->ast->nodes[dim].next_bro;
	}
	r2i_printf(r, "] = ");
	r2i_print_expr(r, (uint32_t)n->u64);
}

void r2i_print_expr (R2i *r, uint32_t idx)
{
	ASTNode *n = r->ir->ast->nodes + idx;

	if (r2i_print_literal(r, n) || r2i_print_symbol(r, n)) return;
	switch (n->type)
	{
	case NODE_CAST: r2i_print_cast(r, n); return;
	case NODE_NEG: r2i_printf(r, "-"); r2i_print_expr(r, n->child); return;
	case NODE_NOT_A: r2i_printf(r, "~"); r2i_print_expr(r, n->child); return;
	case NODE_STRUCT_INIT: case NODE_ARRAY_INIT:
		r2i_print_init_list(r, n->child); return;
	case NODE_FIELD_INIT:
		r2i_printf(r, ".%.*s = ", (int)n->len, n->str);
		r2i_print_expr(r, n->child);
		return;
	case NODE_INDEX_INIT: r2i_print_index_init(r, n); return;
	default:
		r2i_printf(r, "0");
		r->failed = 1;
		error_report(r->sema->err, ERR_ERROR, node_loc(n),
				"cannot write this expression in a .r2i default argument");
		return;
	}
}
