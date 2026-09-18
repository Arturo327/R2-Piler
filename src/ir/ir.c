#include "ir/ir.h"

static uint32_t push_instr (IR *ir, IRInstr instr)
{
	if (ir->instr_count >= ir->instr_cap) {
		uint32_t old_cap = ir->instr_cap;
		ir->instr_cap <<= 1;
		ir->instrs = arena_realloc(ir->arena, ir->instrs,old_cap * sizeof(IRInstr),
				ir->instr_cap * sizeof(IRInstr));
	}

	uint32_t idx = ir->instr_count++;
	ir->instrs[idx] = instr;
	return idx;
}

static uint32_t push_fn (IR *ir, IRFn fn)
{
	if (ir->fn_count >= ir->fn_cap) {
		uint32_t old_cap = ir->fn_cap;
		ir->fn_cap <<= 1;
		ir->fns = arena_realloc(ir->arena, ir->fns, old_cap * sizeof(IRFn),
				ir->fn_cap * sizeof(IRFn));
	}

	uint32_t idx = ir->fn_count++;
	ir->fns[idx] = fn;
	return idx;
}

static uint32_t push_global (IR *ir, IRGlobal global)
{
	if (ir->global_count >= ir->global_cap) {
		uint32_t old_cap = ir->global_cap;
		ir->global_cap <<= 1;
		ir->globals = arena_realloc(ir->arena, ir->globals, old_cap * sizeof(IRGlobal),
				ir->global_cap * sizeof(IRGlobal));
	}

	uint32_t idx = ir->global_count++;
	ir->globals[idx] = global;
	return idx;
}

void ir_init (IR *ir, Arena *arena, AST *ast, SymbolTable *symtab)
{
	ir->arena = arena;
	ir->ast = ast;
	ir->symtab = symtab;
	ir->init_fn = 0;

	ir->instr_count = 0;
	ir->instr_cap = 256;
	ir->instrs = arena_alloc(arena, sizeof(IRInstr) * ir->instr_cap);

	ir->fn_count = 0;
	ir->fn_cap = 16;
	ir->fns = arena_alloc(arena, sizeof(IRFn) * ir->fn_cap);

	ir->global_count = 0;
	ir->global_cap = 16;
	ir->globals = arena_alloc(arena, sizeof(IRGlobal) * ir->global_cap);
}

static uint32_t count_fn_args (IR *ir, uint32_t args_node)
{
	uint32_t param = ir->ast->nodes[args_node].child;
	uint32_t count = 0;

	while (param != NO_NODE) {
		count++;
		param = ir->ast->nodes[param].next_bro;
	}
	return count;
}

static uint32_t declare_fn (IR *ir, ASTNode *n, Symbol *sym)
{
	IRFn fn = {0};
	fn.name = n->str;
	fn.len = n->len;
	fn.ret_type = sym->type;
	fn.param_count = count_fn_args(ir, n->child);
	return push_fn(ir, fn);
}

static uint32_t declare_global (IR *ir, ASTNode *n, Symbol *sym)
{
	IRGlobal global = {0};
	global.name = n->str;
	global.len = n->len;
	global.type = sym->type;
	return push_global(ir, global);
}

static void declare_globals (IR *ir)
{
	uint32_t idx = ir->ast->nodes[0].child;

	while (idx != NO_NODE) {
		ASTNode *n = ir->ast->nodes + idx;
		Symbol *s = ir->symtab->symbols + n->sym;

		if (n->type == NODE_FN_DEC)
			s->ir_id = declare_fn(ir, n, s);
		else if (n->type == NODE_VAR_DEC)
			s->ir_id = declare_global(ir, n, s);

		idx = n->next_bro;
	}
}

static void make_arg_dec (IR *ir, uint32_t dst, uint8_t type)
{
	IRInstr instr = {0};
	instr.op = IR_PARAM;
	instr.dst = dst;
	instr.data_type = type;
	push_instr(ir, instr);
}

static void gen_stmt (IR *ir, uint32_t *reg_count, uint32_t *label_count, ASTNode *n);
static void gen_block (IR *ir, uint32_t *reg_count, uint32_t *label_count, ASTNode *parent);

static void gen_expr (IR *ir, uint32_t *reg_count, uint32_t *label_count, ASTNode *n)
{
	// TODO
}

// TODO: All functions called by gen_stmt

static void gen_stmt (IR *ir, uint32_t *reg_count, uint32_t *label_count, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_VAR_DEC: gen_var_dec(ir, reg_count, label_count, n); return;
	case NODE_BLOCK: gen_block(ir, reg_count, label_count, n); return;
	case NODE_RET: gen_return(ir, reg_count, label_count, n); return;
	case NODE_WHILE: gen_while(ir, reg_count, label_count, n); return;
	case NODE_FOR: gen_for(ir, reg_count, label_count, n); return;
	case NODE_IF: gen_if(ir, reg_count, label_count, n); return;
	case NODE_EMPTY: return;
	default: gen_expr(ir, reg_count, label_count, n); return;
	}
}

static void gen_block (IR *ir, uint32_t *reg_count, uint32_t *label_count, ASTNode *parent)
{
	uint32_t stmt = parent->child;
	while (stmt != NO_NODE) {
		ASTNode *n = ir->ast->nodes + stmt;
		gen_stmt(ir, reg_count, label_count, n);
		stmt = n->next_bro;
	}
}

static void gen_args_dec (IR *ir, uint32_t *reg_count, uint32_t idx)
{
	uint32_t arg = ir->ast->nodes[idx].child;
	while (arg != NO_NODE) {
		ASTNode *n = ir->ast->nodes + idx;
		Symbol *s = ir->symtab->symbols + n->sym;

		s->ir_id = *reg_count++;
		make_arg_dec(ir, s->ir_id, s->type);
		idx = n->next_bro;
	}
}

static void gen_fn (IR *ir, ASTNode *n, uint32_t fn_idx)
{
	uint32_t args = n->child;
	uint32_t ret = ir->ast->nodes[args].next_bro;
	uint32_t body = ir->ast->nodes[ret].next_bro;

	uint32_t reg_count = 0;
	uint32_t label_count = 0;

	ir->fns[fn_idx].start = ir->instr_count;
	gen_args_dec(ir, &reg_count, args);
	gen_block(ir, &reg_count, &label_count, ir->ast->nodes + body);

	ir->fns[fn_idx].count = ir->instr_count - ir->fns[fn_idx].start;
	ir->fns[fn_idx].reg_count = reg_count;
}

static void gen_fns (IR *ir)
{
	uint32_t idx = ir->ast->nodes[0].child;
	while (idx != NO_NODE) {
		ASTNode *n = ir->ast->nodes + idx;
		if (n->type == NODE_FN_DEC)
			gen_fn(ir, n, ir->symtab->symbols[n->sym].ir_id);
		idx = n->next_bro;
	}
}

void ir_gen (IR *ir)
{
	IRFn init_fn = {
		.name = "$init",
		.len = 5
	};
	ir->init_fn = push_fn(ir, init_fn);

	declare_globals(ir);

	gen_fns(ir);
//	gen_init_fn(ir);
}
