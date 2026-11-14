#include "ir/ir.h"

uint32_t agg_root_sym (AST *ast, uint32_t idx)
{
	ASTNode *n = ast->nodes + idx;

	while (n->type == NODE_DOT || (n->type == NODE_INDEX
			&& type_kind(ast->nodes[n->child].data_type) == TK_ARRAY))
		n = ast->nodes + n->child;
	return n->type == NODE_ID ? n->sym : NO_SYMBOL;
}

static void taint (IR *ir, uint32_t idx, int escape)
{
	uint32_t sym = agg_root_sym(ir->ast, idx);

	if (sym == NO_SYMBOL) return;
	if (escape) ir->symtab->symbols[sym].escaped = 1;
	else ir->symtab->symbols[sym].written = 1;
}

static int array_decay (IR *ir, uint32_t idx, uint32_t parent)
{
	ASTNode *n = ir->ast->nodes + idx;
	ASTNode *p = ir->ast->nodes + parent;

	if (n->type != NODE_ID && n->type != NODE_DOT && n->type != NODE_INDEX)
		return 0;
	if (type_kind(n->data_type) != TK_ARRAY) return 0;
	return !(p->type == NODE_INDEX && p->child == idx);
}

static void taint_multi (IR *ir, ASTNode *n)
{
	uint32_t k = n->child;

	while (ir->ast->nodes[k].next_bro != NO_NODE) {
		taint(ir, k, 0);
		k = ir->ast->nodes[k].next_bro;
	}
}

static void scan_node (IR *ir, uint32_t idx, uint32_t parent)
{
	ASTNode *n = ir->ast->nodes + idx;

	if (n->type >= NODE_ASSIGN && n->type <= NODE_DEC) taint(ir, n->child, 0);
	else if (n->type == NODE_MULTI_ASSIGN) taint_multi(ir, n);
	else if (n->type == NODE_ADDR) taint(ir, n->child, 1);
	else if (array_decay(ir, idx, parent)) taint(ir, idx, 1);

	for (uint32_t k = n->child; k != NO_NODE; k = ir->ast->nodes[k].next_bro)
		scan_node(ir, k, idx);
}

void ir_scan_structs (IR *ir)
{
	uint32_t idx = ir->ast->nodes[0].child;

	while (idx != NO_NODE) {
		if (ir->ast->nodes[idx].type == NODE_FN_DEC)
			scan_node(ir, idx, 0);
		idx = ir->ast->nodes[idx].next_bro;
	}
}
