#include "ir/ir.h"

#include <assert.h>
#include <stdio.h>

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

static const IRInstr blank_instr = {
	.dst = NO_REG,
	.src1 = NO_REG,
	.src2 = NO_REG
};

static const uint8_t node_to_ir[NODE_COUNT] = {
	[NODE_ADD] = IR_ADD, [NODE_SUB] = IR_SUB, [NODE_MUL] = IR_MUL,
	[NODE_DIV] = IR_DIV, [NODE_MOD] = IR_MOD,
	[NODE_AND_A] = IR_AND_A, [NODE_OR_A] = IR_OR_A, [NODE_XOR] = IR_XOR,
	[NODE_RS] = IR_RS, [NODE_LS] = IR_LS,
	[NODE_NEG] = IR_NEG, [NODE_NOT_A] = IR_NOT_A, [NODE_NOT_L] = IR_NOT_L,
	[NODE_EQ] = IR_EQ, [NODE_NE] = IR_NE, [NODE_GT] = IR_GT,
	[NODE_GE] = IR_GE, [NODE_LT] = IR_LT, [NODE_LE] = IR_LE
};

static const uint8_t char_wraps[IR_COUNT] = {
	[IR_ADD] = 1, [IR_SUB] = 1, [IR_MUL] = 1, [IR_DIV] = 1,
	[IR_LS] = 1, [IR_NEG] = 1
};

void ir_init (IR *ir, Arena *arena, Sema *sema)
{
	ir->arena = arena;
	ir->ast = sema->ast;
	ir->symtab = &sema->table;
	ir->init_order = sema->init_order;
	ir->init_order_count = sema->init_order_count;
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

static uint32_t count_bros (IR *ir, uint32_t first)
{
	uint32_t count = 0;

	while (first != NO_NODE) {
		count++;
		first = ir->ast->nodes[first].next_bro;
	}
	return count;
}

static uint32_t declare_fn (IR *ir, ASTNode *n, Symbol *sym)
{
	IRFn fn = {0};
	fn.name = n->str;
	fn.len = n->len;
	fn.ret_type = sym->type;
	fn.param_count = count_bros(ir, ir->ast->nodes[n->child].child);
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

static void make_arg_dec (IR *ir, uint32_t dst, uint32_t idx, uint8_t type)
{
	IRInstr i = blank_instr;
	i.op = IR_PARAM;
	i.dst = dst;
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
}

static void make_arg (IR *ir, uint32_t val, uint32_t idx)
{
	IRInstr i = blank_instr;
	i.op = IR_ARG;
	i.src1 = val;
	i.target = idx;
	push_instr(ir, i);
}

static void make_const_into (IR *ir, uint32_t dst, int64_t val, uint8_t type)
{
	IRInstr i = blank_instr;
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
	IRInstr i = blank_instr;
	i.op = IR_LD_GLOBAL;
	i.dst = ir->reg_count++;
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
	return i.dst;
}

static void make_str_global (IR *ir, uint32_t idx, uint32_t val, uint8_t type)
{
	IRInstr i = blank_instr;
	i.op = IR_STR_GLOBAL;
	i.src1 = val;
	i.target = idx;
	i.data_type = type;
	push_instr(ir, i);
}

static uint32_t make_call (IR *ir, uint32_t idx, uint16_t argc, uint8_t ret_type)
{
	IRInstr i = blank_instr;
	i.op = IR_CALL;
	i.dst = ret_type == TYPE_VOID ? NO_REG : ir->reg_count++;
	i.target = idx;
	i.argc = argc;
	i.data_type = ret_type;
	push_instr(ir, i);
	return i.dst;
}

static void make_move (IR *ir, uint32_t dst, uint32_t src, uint8_t type)
{
	IRInstr i = blank_instr;
	i.op = IR_MOVE;
	i.dst = dst;
	i.src1 = src;
	i.data_type = type;
	push_instr(ir, i);
}

static uint32_t make_op (IR *ir, uint8_t op, uint32_t a, uint32_t b, uint8_t type)
{
	IRInstr i = blank_instr;
	i.op = op;
	i.dst = ir->reg_count++;
	i.src1 = a;
	i.src2 = b;
	i.data_type = type;
	push_instr(ir, i);

	if (types[type].size >= 8) return i.dst;
	if (!char_wraps[op] && !(op == IR_NOT_A && !types[type].sign)) return i.dst;

	IRInstr fix = blank_instr;
	fix.op = IR_EXTEND;
	fix.dst = ir->reg_count++;
	fix.src1 = i.dst;
	fix.data_type = type;
	push_instr(ir, fix);
	return fix.dst;
}

static void make_jump (IR *ir, uint8_t op, uint32_t cond, uint32_t label)
{
	IRInstr i = blank_instr;
	i.op = op;
	i.src1 = cond;
	i.target = label;
	push_instr(ir, i);
}

static void make_label (IR *ir, uint32_t label)
{
	IRInstr i = blank_instr;
	i.op = IR_LABEL;
	i.target = label;
	push_instr(ir, i);
}

static void make_ret (IR *ir, uint32_t val, uint8_t type)
{
	IRInstr i = blank_instr;
	i.op = IR_RET;
	i.src1 = val;
	i.data_type = type;
	push_instr(ir, i);
}

static void gen_stmt (IR *ir, ASTNode *n);
static void gen_block (IR *ir, ASTNode *parent);
static uint32_t gen_expr (IR *ir, ASTNode *n);
static void gen_jump_if (IR *ir, ASTNode *n, uint32_t label, int when);

static uint32_t gen_id (IR *ir, ASTNode *n)
{
	Symbol *s = ir->symtab->symbols + n->sym;
	uint32_t tmp;

	if (!s->depth)
		return make_ld_global(ir, s->ir_id, s->type);

	tmp = ir->reg_count++;
	make_move(ir, tmp, s->ir_id, s->type);
	return tmp;
}

static uint32_t gen_call (IR *ir, ASTNode *n)
{
	Symbol *fn = ir->symtab->symbols + n->sym;
	uint32_t argc = count_bros(ir, n->child);
	uint32_t regs[argc + 1];
	uint32_t arg = n->child;

	for (uint32_t i = 0; i < argc; i++) {
		regs[i] = gen_expr(ir, ir->ast->nodes + arg);
		arg = ir->ast->nodes[arg].next_bro;
	}
	for (uint32_t i = 0; i < argc; i++)
		make_arg(ir, regs[i], i);

	return make_call(ir, fn->ir_id, (uint16_t)argc, fn->type);
}

static uint32_t gen_assign (IR *ir, ASTNode *n)
{
	ASTNode *lhs = ir->ast->nodes + n->child;
	Symbol *s = ir->symtab->symbols + lhs->sym;
	uint32_t val = gen_expr(ir, ir->ast->nodes + lhs->next_bro);

	if (!s->depth) make_str_global(ir, s->ir_id, val, s->type);
	else make_move(ir, s->ir_id, val, s->type);
	return val;
}

static uint32_t gen_unary (IR *ir, ASTNode *n)
{
	ASTNode *operand = ir->ast->nodes + n->child;
	uint32_t a = gen_expr(ir, operand);

	return make_op(ir, node_to_ir[n->type], a, NO_REG, operand->data_type);
}

static uint32_t gen_binary (IR *ir, ASTNode *n)
{
	ASTNode *l = ir->ast->nodes + n->child;
	ASTNode *r = ir->ast->nodes + l->next_bro;
	uint32_t a = gen_expr(ir, l);
	uint32_t b = gen_expr(ir, r);

	return make_op(ir, node_to_ir[n->type], a, b, l->data_type);
}

static void gen_jump_logic (IR *ir, ASTNode *n, uint32_t label, int when)
{
	ASTNode *lhs = ir->ast->nodes + n->child;
	ASTNode *rhs = ir->ast->nodes + lhs->next_bro;
	int stop = n->type == NODE_OR_L;
	uint32_t skip;

	if (when == stop) {
		gen_jump_if(ir, lhs, label, stop);
		gen_jump_if(ir, rhs, label, when);
		return;
	}
	skip = ir->label_count++;
	gen_jump_if(ir, lhs, skip, stop);
	gen_jump_if(ir, rhs, label, when);
	make_label(ir, skip);
}

static void gen_jump_if (IR *ir, ASTNode *n, uint32_t label, int when)
{
	if (n->type == NODE_NOT_L) {
		gen_jump_if(ir, ir->ast->nodes + n->child, label, !when);
	} else if (n->type == NODE_AND_L || n->type == NODE_OR_L) {
		gen_jump_logic(ir, n, label, when);
	} else {
		uint32_t reg = gen_expr(ir, n);
		make_jump(ir, when ? IR_JNZ : IR_JZ, reg, label);
	}
}

static uint32_t gen_logic (IR *ir, ASTNode *n)
{
	uint32_t res = ir->reg_count++;
	uint32_t l_false = ir->label_count++;
	uint32_t l_end = ir->label_count++;

	gen_jump_if(ir, n, l_false, 0);
	make_const_into(ir, res, 1, TYPE_i64);
	make_jump(ir, IR_JMP, NO_REG, l_end);
	make_label(ir, l_false);
	make_const_into(ir, res, 0, TYPE_i64);
	make_label(ir, l_end);
	return res;
}

static uint32_t gen_cast (IR *ir, ASTNode *n)
{
	ASTNode *operand = ir->ast->nodes + n->child;
	uint32_t src = gen_expr(ir, operand);

	if (types[n->data_type].size == 8 || n->data_type == operand->data_type)
		return src;
	return make_op(ir, IR_EXTEND, src, NO_REG, n->data_type);
}

static uint32_t gen_expr (IR *ir, ASTNode *n)
{
	switch (n->type)
	{
	case NODE_LIT_i64: return make_const(ir, n->i64, n->data_type);
	case NODE_LIT_u64: return make_const(ir, (int64_t)n->u64, n->data_type);
	case NODE_LIT_CHAR: return make_const(ir, (int8_t)n->chr, n->data_type);

	case NODE_ID: return gen_id(ir, n);
	case NODE_FN_CALL: return gen_call(ir, n);
	case NODE_ASSIGN: return gen_assign(ir, n);

	case NODE_NEG: case NODE_NOT_A: case NODE_NOT_L: return gen_unary(ir, n);
	case NODE_AND_L: case NODE_OR_L: return gen_logic(ir, n);
	case NODE_CAST: return gen_cast(ir, n);

	default:
		assert(node_to_ir[n->type] != IR_NOP);
		return gen_binary(ir, n);
	}
}

static void gen_var_dec (IR *ir, ASTNode *n)
{
	Symbol *s = ir->symtab->symbols + n->sym;

	if (n->child == NO_NODE) s->ir_id = ir->reg_count++;
	else s->ir_id = gen_expr(ir, ir->ast->nodes + n->child);
}

static void gen_return (IR *ir, ASTNode *n)
{
	uint32_t reg = NO_REG;
	uint8_t type = TYPE_VOID;

	if (n->child != NO_NODE) {
		ASTNode *val = ir->ast->nodes + n->child;
		reg = gen_expr(ir, val);
		type = val->data_type;
	}
	make_ret(ir, reg, type);
}

static void gen_branch (IR *ir, uint32_t cond, uint32_t body, uint32_t end, int more)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t next = ir->label_count++;

	gen_jump_if(ir, nodes + cond, next, 0);
	gen_stmt(ir, nodes + body);
	if (more) make_jump(ir, IR_JMP, NO_REG, end);
	make_label(ir, next);
}

static void gen_if (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t cond = n->child;
	uint32_t body = nodes[cond].next_bro;
	uint32_t branch = nodes[body].next_bro;
	uint32_t end = ir->label_count++;

	gen_branch(ir, cond, body, end, branch != NO_NODE);
	while (branch != NO_NODE) {
		ASTNode *b = nodes + branch;
		int more = b->next_bro != NO_NODE;

		if (b->type == NODE_ELSE) gen_stmt(ir, nodes + b->child);
		else gen_branch(ir, b->child, nodes[b->child].next_bro, end, more);
		branch = b->next_bro;
	}
	make_label(ir, end);
}

static void gen_while (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	uint32_t body = nodes[n->child].next_bro;
	uint32_t l_cond = ir->label_count++;
	uint32_t l_end = ir->label_count++;

	make_label(ir, l_cond);
	gen_jump_if(ir, nodes + n->child, l_end, 0);
	gen_stmt(ir, nodes + body);
	make_jump(ir, IR_JMP, NO_REG, l_cond);
	make_label(ir, l_end);
}

static void gen_for (IR *ir, ASTNode *n)
{
	ASTNode *nodes = ir->ast->nodes;
	ASTNode *cond = nodes + nodes[n->child].next_bro;
	ASTNode *updt = nodes + cond->next_bro;
	ASTNode *body = nodes + updt->next_bro;
	uint32_t l_cond = ir->label_count++;
	uint32_t l_end = ir->label_count++;

	gen_stmt(ir, nodes + n->child);
	make_label(ir, l_cond);
	if (cond->type != NODE_EMPTY)
		gen_jump_if(ir, cond, l_end, 0);

	gen_stmt(ir, body);
	gen_stmt(ir, updt);
	make_jump(ir, IR_JMP, NO_REG, l_cond);
	make_label(ir, l_end);
}

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

static void gen_args_dec (IR *ir, uint32_t node)
{
	uint32_t p = ir->ast->nodes[node].child;
	uint32_t idx = 0;

	while (p != NO_NODE) {
		ASTNode *n = ir->ast->nodes + p;
		Symbol *s = ir->symtab->symbols + n->sym;

		s->ir_id = ir->reg_count++;
		make_arg_dec(ir, s->ir_id, idx++, s->type);
		p = n->next_bro;
	}
}

static void fn_end (IR *ir, uint32_t idx)
{
	IRFn *fn = ir->fns + idx;

	make_ret(ir, NO_REG, TYPE_VOID);
	fn->count = ir->instr_count - fn->start;
	fn->reg_count = ir->reg_count;
}

static void gen_fn (IR *ir, ASTNode *n, uint32_t fn_idx)
{
	uint32_t args = n->child;
	uint32_t ret = ir->ast->nodes[args].next_bro;
	uint32_t body = ir->ast->nodes[ret].next_bro;

	ir->reg_count = 0;
	ir->fns[fn_idx].start = ir->instr_count;
	gen_args_dec(ir, args);
	gen_block(ir, ir->ast->nodes + body);
	fn_end(ir, fn_idx);
}

static void gen_init_fn (IR *ir)
{
	ir->reg_count = 0;
	ir->fns[ir->init_fn].start = ir->instr_count;

	for (uint32_t i = 0; i < ir->init_order_count; i++) {
		ASTNode *n = ir->ast->nodes + ir->init_order[i];
		Symbol *s = ir->symtab->symbols + n->sym;
		uint32_t val = gen_expr(ir, ir->ast->nodes + n->child);

		make_str_global(ir, s->ir_id, val, s->type);
	}
	fn_end(ir, ir->init_fn);
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
		.name = INIT_FN_NAME,
		.len = sizeof(INIT_FN_NAME) - 1,
		.ret_type = TYPE_VOID
	};

	ir->init_fn = push_fn(ir, init_fn);
	declare_globals(ir);
	gen_init_fn(ir);
	gen_fns(ir);
}

// debug shit

static const char *const op_names[IR_COUNT] = {
	[IR_NOP] = "nop", [IR_CONST] = "const", [IR_PARAM] = "param",
	[IR_LD_GLOBAL] = "ld_global", [IR_STR_GLOBAL] = "str_global",
	[IR_MOVE] = "move", [IR_EXTEND] = "extend",
	[IR_ADD] = "add", [IR_SUB] = "sub", [IR_MUL] = "mul",
	[IR_DIV] = "div", [IR_MOD] = "mod",
	[IR_AND_A] = "and", [IR_OR_A] = "or", [IR_XOR] = "xor",
	[IR_RS] = "rshift", [IR_LS] = "lshift",
	[IR_NEG] = "neg", [IR_NOT_A] = "not", [IR_NOT_L] = "lnot",
	[IR_EQ] = "eq", [IR_NE] = "ne", [IR_GT] = "gt", [IR_GE] = "ge",
	[IR_LT] = "lt", [IR_LE] = "le",
	[IR_LABEL] = "label", [IR_JMP] = "jmp", [IR_JZ] = "jz", [IR_JNZ] = "jnz",
	[IR_ARG] = "arg", [IR_CALL] = "call", [IR_RET] = "ret"
};

static void dump_extra (IR *ir, IRInstr *i)
{
	switch (i->op)
	{
	case IR_CONST:
		if (types[i->data_type].sign) printf(" %lld", (long long)i->imm64);
		else printf(" %llu", (unsigned long long)i->imm64);
		break;
	case IR_JMP: case IR_JZ: case IR_JNZ:
		printf(" -> L%u", i->target);
		break;
	case IR_PARAM: case IR_ARG:
		printf(" #%u", i->target);
		break;
	case IR_CALL:
		printf(" %.*s", (int)ir->fns[i->target].len, ir->fns[i->target].name);
		break;
	case IR_LD_GLOBAL: case IR_STR_GLOBAL:
		printf(" @%.*s", (int)ir->globals[i->target].len, ir->globals[i->target].name);
		break;
	default:
		break;
	}
}

static void dump_instr (IR *ir, IRInstr *i)
{
	if (i->op == IR_LABEL) {
		printf("  L%u:\n", i->target);
		return;
	}

	printf("    ");
	if (i->dst != NO_REG)
		printf("r%u = ", i->dst);
	printf("%s", op_names[i->op]);
	if (i->data_type != TYPE_VOID)
		printf(".%s", types[i->data_type].name);
	if (i->src1 != NO_REG)
		printf(" r%u", i->src1);
	if (i->src2 != NO_REG)
		printf(", r%u", i->src2);
	dump_extra(ir, i);
	printf("\n");
}

static void dump_fn (IR *ir, uint32_t idx)
{
	IRFn *fn = ir->fns + idx;

	printf("fn %.*s(%u params, %u regs) : %s\n", (int)fn->len, fn->name,
			fn->param_count, fn->reg_count, types[fn->ret_type].name);
	for (uint32_t k = 0; k < fn->count; k++)
		dump_instr(ir, ir->instrs + fn->start + k);
	printf("\n");
}

void dump_ir (IR *ir)
{
	for (uint32_t i = 0; i < ir->global_count; i++)
		printf("global %.*s : %s\n", (int)ir->globals[i].len, ir->globals[i].name,
				types[ir->globals[i].type].name);
	if (ir->global_count)
		printf("\n");

	for (uint32_t i = 0; i < ir->fn_count; i++)
		dump_fn(ir, i);
}
