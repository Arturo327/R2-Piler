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
	ir->reg_count = 0;
	ir->label_count = 0;

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
	IRInstr i = {0};
	i.op = IR_PARAM;
	i.dst = dst;
	i.data_type = type;
	push_instr(ir, i);
}

static void make_arg (IR *ir, uint32_t val)
{
	IRInstr i = {0};
	i.op = IR_ARG;
	i.dst = NO_REG;
	i.src1 = val;
	push_instr(ir, i);
}

static void make_const_into (IR *ir, uint32_t dst, int64_t val, uint8_t type)
{
	IRInstr i = {0};
	i.op = IR_CONST;
	i.dst = dst;
	i.imm64 = val;
	i.data_type = type;
	push_instr(ir, i);
}

static uint32_t make_const (IR *ir, int64_t val, uint8_t type)
{
	uint32_t dst = ir->reg_count++;
	make_const_into(ir, dst, val, type);
	return dst;
}

static uint32_t make_ld_global (IR *ir, uint32_t idx, uint8_t type)
{
	IRInstr i = {0};
	i.op = IR_LD_GLOBAL;
	i.dst = ir->reg_count++;
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
	return i.dst;
}

static void make_str_global (IR *ir, uint32_t idx, uint32_t value, uint8_t type)
{
	IRInstr i = {0};
	i.op = IR_STR_GLOBAL;
	i.dst = NO_REG;
	i.src1 = value;
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
}

static uint32_t make_call (IR *ir, uint32_t idx, uint16_t argc, uint8_t ret_type)
{
	IRInstr i = {0};
	i.op = IR_CALL;
	i.dst = ir->reg_count++;
	i.target = idx;
	i.argc = argc;
	i.data_type = ret_type;
	push_instr(ir, i);
	return i.dst;
}

static void make_move (IR *ir, uint32_t dst, uint32_t src)
{
	IRInstr i = {0};
	i.op = IR_MOVE;
	i.dst = dst;
	i.src1 = src;
	push_instr(ir, i);
}

static void gen_stmt (IR *ir, ASTNode *n);
static void gen_block (IR *ir, ASTNode *parent);
static uint32_t gen_expr (IR *ir, ASTNode *n);

static uint32_t gen_id (IR *ir, ASTNode *n)
{
	Symbol *s = ir->symtab->symbols + n->sym;

	if (!s->depth)
		return make_ld_global(ir, s->ir_id, s->type);

	return s->ir_id;
}

static uint32_t gen_call (IR *ir, ASTNode *n)
{
	Symbol *fn = ir->symtab->symbols + n->sym;
	uint32_t arg = n->child;
	uint16_t argc = 0;

	while (arg != NO_NODE) {
		uint32_t reg = gen_expr(ir, ir->ast->nodes + arg);
		make_arg(ir, reg);
		arg = ir->ast->nodes[arg].next_bro;
		argc++;
	}

	return make_call(ir, fn->ir_id, argc, fn->type);
}

static uint32_t gen_assign (IR *ir, ASTNode *n)
{
	ASTNode *lhs = ir->ast->nodes + n->child;
	Symbol *s = ir->symtab->symbols + lhs->sym;
	uint32_t val = gen_expr(ir, ir->ast->nodes + lhs->next_bro);

	if (!s->depth) make_str_global(ir, s->ir_id, val, s->type);
	else make_move(ir, s->ir_id, val);

	return val;
}

// TODO: Some functions called by gen_expr

static uint32_t gen_expr (IR *ir, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_LIT_i64: return make_const(ir, n->i64, TYPE_i64);
	case NODE_LIT_u64: return make_const(ir, n->u64, TYPE_u64);
	case NODE_LIT_CHAR: return make_const(ir, n->chr, TYPE_CHAR);

	case NODE_ID: return gen_id(ir, n);
	case NODE_FN_CALL: return gen_call(ir, n);
	case NODE_ASSIGN: return gen_assign(ir, n);

	case NODE_NEG: case NODE_NOT_A: case NODE_NOT_L: return gen_unary(ir, n);
	case NODE_AND_L: return gen_and(ir, n);
	case NODE_OR_L: return gen_or(ir, n);

	default: return gen_binary(ir, n);
	}
}

// TODO: All functions called by gen_stmt

static void gen_stmt (IR *ir, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_VAR_DEC: gen_var_dec(ir, n); return;
	case NODE_BLOCK: gen_block(ir, n); return;
	case NODE_RET: gen_return(ir, n); return;
	case NODE_WHILE: gen_while(ir, n); return;
	case NODE_FOR: gen_for(ir, n); return;
	case NODE_IF: gen_if(ir, n); return;
	case NODE_EMPTY: return;
	default: gen_expr(ir, n); return;
	}
}

static void gen_block (IR *ir, ASTNode *parent)
{
	uint32_t stmt = parent->child;
	while (stmt != NO_NODE) {
		ASTNode *n = ir->ast->nodes + stmt;
		gen_stmt(ir, n);
		stmt = n->next_bro;
	}
}

static void gen_args_dec (IR *ir, uint32_t idx)
{
	uint32_t arg = ir->ast->nodes[idx].child;
	while (arg != NO_NODE) {
		ASTNode *n = ir->ast->nodes + idx;
		Symbol *s = ir->symtab->symbols + n->sym;

		s->ir_id = ir->reg_count++;
		make_arg_dec(ir, s->ir_id, s->type);
		idx = n->next_bro;
	}
}

static void gen_fn (IR *ir, ASTNode *n, uint32_t fn_idx)
{
	uint32_t args = n->child;
	uint32_t ret = ir->ast->nodes[args].next_bro;
	uint32_t body = ir->ast->nodes[ret].next_bro;

	ir->reg_count = 0;
	ir->label_count = 0;

	ir->fns[fn_idx].start = ir->instr_count;
	gen_args_dec(ir, args);
	gen_block(ir, ir->ast->nodes + body);

	ir->fns[fn_idx].count = ir->instr_count - ir->fns[fn_idx].start;
	ir->fns[fn_idx].reg_count = ir->reg_count;
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
