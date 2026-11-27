#include "r2i/internal.h"

#include "parser/operators.h"

static void r2i_make_param (R2i *r, ASTNode *n, int variadic)
{
	if (n->str) r2i_printf(r, "%.*s", (int)n->len, n->str);
	if (n->flags & NODE_FLAG_SELF) return;
	if (n->str) r2i_printf(r, " : ");
	if (n->flags & NODE_FLAG_REF_LOWERED) {
		r2i_printf(r, "&");
		r2i_print_type(r, types[n->data_type].base);
	} else if (variadic) {
		r2i_printf(r, "...");
		r2i_print_type(r, types[n->data_type].base);
	} else {
		r2i_print_type(r, n->data_type);
	}
	if (n->child != NO_NODE) {
		r2i_printf(r, " = ");
		r2i_print_expr(r, n->child);
	}
}

static void r2i_make_params (R2i *r, ASTNode *args)
{
	int typed = (args->flags & NODE_FLAG_VARARG_TYPED) != 0;
	uint32_t p = args->child;
	int first = 1;

	while (p != NO_NODE) {
		ASTNode *n = r->ir->ast->nodes + p;
		int variadic = typed && n->next_bro == NO_NODE;

		if (!first) r2i_printf(r, ", ");
		first = 0;
		r2i_make_param(r, n, variadic);
		p = n->next_bro;
	}
	if (args->flags & NODE_FLAG_VARARG) {
		if (!first) r2i_printf(r, ", ");
		r2i_printf(r, "...");
	}
}

static void r2i_make_ret (R2i *r, ASTNode *ret)
{
	r2i_printf(r, " : ");
	r2i_print_type(r, ret->data_type);
	for (uint32_t item = ret->child; item != NO_NODE;
			item = r->ir->ast->nodes[item].next_bro) {
		r2i_printf(r, ", ");
		r2i_print_type(r, r->ir->ast->nodes[item].data_type);
	}
}

void r2i_make_fn (R2i *r, ASTNode *n, int is_extern)
{
	Symbol *sym = r->ir->symtab->symbols + n->sym;
	IRFn *fn = r->ir->fns + sym->ir_id;
	ASTNode *args = r->ir->ast->nodes + n->child;
	ASTNode *ret = r->ir->ast->nodes + args->next_bro;

	if (n->flags & NODE_FLAG_EXPORT)
		r2i_printf(r, "export fn %s (", fn->sym);
	else if (is_extern)
		r2i_printf(r, "pub extern fn %s (", fn->sym);
	else r2i_printf(r, "pub fn %s (", fn->sym);
	r2i_make_params(r, args);
	r2i_printf(r, ")");
	r2i_make_ret(r, ret);
	r2i_printf(r, ";\n\n");
}

void r2i_make_global (R2i *r, ASTNode *n, int is_extern)
{
	Symbol *sym = r->ir->symtab->symbols + n->sym;
	IRGlobal *g = r->ir->globals + sym->ir_id;
	const char *kind = n->type == NODE_ARR_DEC ? "arr" : "var";

	if (n->flags & NODE_FLAG_EXPORT)
		r2i_printf(r, "export %s %s : ", kind, g->sym);
	else if (is_extern)
		r2i_printf(r, "pub extern %s %s : ", kind, g->sym);
	else r2i_printf(r, "pub %s %s : ", kind, g->sym);
	r2i_print_type(r, n->data_type);
	r2i_printf(r, ";\n\n");
}

void r2i_make_method (R2i *r, ASTNode *n)
{
	ASTNode *args = r->ir->ast->nodes + n->child;
	ASTNode *ret = r->ir->ast->nodes + args->next_bro;

	if (n->op != OP_NONE) r2i_printf(r, "op %s (", op_spelling(n->op));
	else if (n->flags & NODE_FLAG_INIT) r2i_printf(r, "init (");
	else if (n->flags & NODE_FLAG_DESTROY) r2i_printf(r, "destroy (");
	else r2i_printf(r, "fn %.*s (", (int)n->len, n->str);

	r2i_make_params(r, args);
	r2i_printf(r, ")");
	if (!(n->flags & (NODE_FLAG_INIT | NODE_FLAG_DESTROY)))
		r2i_make_ret(r, ret);
	r2i_printf(r, ";\n");
}

static void r2i_make_decl (R2i *r, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_FN_DEC: r2i_make_fn(r, n, 0); return;
	case NODE_CLASS_DEC: r2i_make_class(r, n); return;
	case NODE_EXTERN_FN_DEC: r2i_make_fn(r, n, 1); return;
	case NODE_VAR_DEC: case NODE_ARR_DEC: r2i_make_global(r, n, 0); return;
	case NODE_EXTERN_VAR_DEC: r2i_make_global(r, n, 1); return;
	case NODE_STRUCT_DEC: r2i_make_struct(r, n); return;
	case NODE_ENUM_DEC: r2i_make_enum(r, n); return;
	case NODE_TYPE_DEC: r2i_make_alias(r, n, 0); return;
	case NODE_NEWTYPE_DEC: r2i_make_alias(r, n, 1); return;
	default: return;
	}
}

void r2i_make_decls (R2i *r)
{
	AST *ast = r->ir->ast;

	for (uint32_t idx = ast->nodes[0].child; idx != NO_NODE;
			idx = ast->nodes[idx].next_bro) {
		if ((ast->nodes[idx].flags & NODE_FLAG_PUB)
				&& !(ast->nodes[idx].flags & NODE_FLAG_IMPORTED))
			r2i_make_decl(r, ast->nodes + idx);
	}
}

void r2i_make_init (R2i *r)
{
	if (!r2i_has_init(r->ir)) return;
	r2i_printf(r, "pub fn %s () : void;\n", r->ir->fns[r->ir->init_fn].sym);
}
