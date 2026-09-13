#include "sema/sema.h"

#include <stdio.h>

static ErrorLoc node_loc (ASTNode *n)
{
	ErrorLoc loc = {
		.line = n->line,
		.col = n->col,
		.len = n->len ? n->len : 1
	};
	return loc;
}

static const char *type_name (uint8_t type)
{
	switch (type)
	{
	case TYPE_VOID: return "void";
	case TYPE_i64:  return "i64";
	case TYPE_u64:  return "u64";
	case TYPE_CHAR: return "char";
	default:        return "unknown";
	}
}

void sema_init (Sema *s, Arena *arena, AST *ast, ErrorReporter *err)
{
	s->ast = ast;
	s->err = err;
	s->arena = arena;
	s->depth = 0;
	s->curr_ret = TYPE_VOID;
	s->in_fn = 0;

	symtab_init(&s->table, arena);
}

static uint32_t sema_declare (Sema *s, char *name, uint16_t len, SymKind kind,
		uint8_t data_type, uint32_t decl_node)
{
	uint32_t prev = symtab_find(&s->table, name, len);

	if (prev != NO_SYMBOL && s->table.symbols[prev].depth == s->depth) {
		ASTNode *n = &s->ast->nodes[decl_node];
		error_report(s->err, ERR_ERROR, node_loc(n),
				"'%.*s' already declared in this scope", (int)len, name);
	}

	Symbol sym = {
		.name = name,
		.len = len,
		.line = s->ast->nodes[decl_node].line,
		.col = s->ast->nodes[decl_node].col,
		.decl = decl_node,
		.depth = s->depth,
		.kind = (uint8_t)kind,
		.type = data_type
	};

	uint32_t sym_idx = symtab_declare(&s->table, s->arena, sym);
	s->ast->nodes[decl_node].sym = sym_idx;
	return sym_idx;
}

static void check_root (Sema *s)
{
	(void)s;
}

static void decl_fns (Sema *s)
{
	uint32_t idx = s->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = &s->ast->nodes[idx];
		if (n->type == NODE_FN_DEC) {
			uint32_t args = n->child;
			uint32_t ret = s->ast->nodes[args].next_bro;
			uint8_t data_type = s->ast->nodes[ret].data_type;
			sema_declare(s, n->str, n->len, SYMBOL_FN, data_type, idx);
		}
		idx = n->next_bro;
	}
}

void sema_run (Sema *s)
{
	decl_fns(s);
	check_root(s);
}

static const char *kind_name (uint8_t kind)
{
	switch (kind)
	{
	case SYMBOL_FN:    return "fn";
	case SYMBOL_VAR:   return "var";
	case SYMBOL_PARAM: return "param";
	default:           return "unknown";
	}
}

void dump_symbols (SymbolTable *t)
{
	for (uint32_t i = 0; i < t->count; i++) {
		Symbol *sym = &t->symbols[i];
		printf("%s %.*s [%u:%u] depth=%u type=%s\n",
				kind_name(sym->kind), (int)sym->len, sym->name,
				sym->line, sym->col, sym->depth, type_name(sym->type));
	}
}
