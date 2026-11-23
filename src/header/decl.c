#include "header/internal.h"

#include <string.h>

static void h_make_param (CHeader *h, ASTNode *n, int names)
{
	TypeId type = n->data_type;

	if (type_kind(type) == TK_ARRAY) {
		TypeId leaf = type_flat_element(type);

		if (type_is_const(type) && !type_is_const(leaf)) leaf = type_const_of(leaf);
		h_print_type(h, leaf);
		h_printf(h, " *");
	} else h_print_type(h, type);
	if (names && n->str)
		h_printf(h, " %.*s", (int)n->len, n->str);
}

static void h_make_params (CHeader *h, uint32_t args)
{
	AST *ast = h->ir->ast;
	int first = 1;

	for (uint32_t p = ast->nodes[args].child; p != NO_NODE;
			p = ast->nodes[p].next_bro) {
		if (!first) h_printf(h, ", ");
		first = 0;
		h_make_param(h, ast->nodes + p, 1);
	}
	if (first) h_printf(h, "void");
}

static void h_make_fn (CHeader *h, ASTNode *n)
{
	Symbol *sym = h->ir->symtab->symbols + n->sym;
	IRFn *fn = h->ir->fns + sym->ir_id;
	AST *ast = h->ir->ast;
	uint32_t args = n->child;
	ASTNode *ret = ast->nodes + ast->nodes[args].next_bro;

	h_printf(h, "R2_API ");
	h_print_type(h, ret->data_type);
	h_printf(h, " %s (", fn->sym);
	h_make_params(h, args);
	h_printf(h, ");\n\n");
}

static void h_make_global (CHeader *h, ASTNode *n)
{
	Symbol *sym = h->ir->symtab->symbols + n->sym;
	IRGlobal *g = h->ir->globals + sym->ir_id;

	h_printf(h, "R2_API extern ");
	h_print_named_type(h, n->data_type, g->sym, (int)strlen(g->sym));
	h_printf(h, ";\n\n");
}

static void h_make_init (CHeader *h)
{
	h_printf(h, "#if %s\n", h->init_macro);
	h_printf(h, "R2_API extern void %s (void);\n", h->init_sym);
	h_printf(h, "#endif\n\n");
}

static void h_make_epilogue (CHeader *h)
{
	h_printf(h, "#ifdef __cplusplus\n}\n#endif\n");
	h_printf(h, "#endif /* %s */\n", h->guard);
}

void h_make_decls (CHeader *h)
{
	AST *ast = h->ir->ast;

	for (uint32_t idx = ast->nodes[0].child; idx != NO_NODE;
			idx = ast->nodes[idx].next_bro) {
		ASTNode *n = ast->nodes + idx;

		if (!(n->flags & NODE_FLAG_EXPORT)) continue;
		switch (n->type)
		{
		case NODE_FN_DEC:
			h_make_fn(h, n);
			break;
		case NODE_VAR_DEC: case NODE_ARR_DEC:
			h_make_global(h, n);
			break;
		default:
			break;
		}
	}
	h_make_init(h);
	h_make_epilogue(h);
}
