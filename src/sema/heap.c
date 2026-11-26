#include "sema/common.h"
#include "sema/expr_internal.h"

#include <string.h>

#define STDLIB_LEN 6

static int stdlib_imported (Sema *s)
{
	for (uint32_t i = 0; i < s->module_count; i++) {
		const ImportModule *m = s->modules + i;
		if (m->name_len < STDLIB_LEN) continue;
		const char *tail = m->name + m->name_len - STDLIB_LEN;
		if (memcmp(tail, "stdlib", STDLIB_LEN)) continue;
		if (m->name_len == STDLIB_LEN || tail[-1] == '/') return 1;
	}
	return 0;
}

static void require_stdlib (Sema *s, ASTNode *n, const char *what)
{
	if (stdlib_imported(s)) return;
	error_report(s->err, ERR_ERROR, node_loc(n),
			"'%s' requires the R2 standard library; add 'import \"stdlib\";'", what);
}

static void check_allocator (Sema *s, uint32_t alloc)
{
	TypeId type = check_expr(s, alloc);

	if (type != TYPE_ERROR && type_kind(type) != TK_PTR)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + alloc),
				"the allocator expression of 'new' must be a pointer, not %s",
				type_display_name(type));
}

static int class_slot_ok (TypeId slot, TypeId cls)
{
	TypeId ptr = type_ptr_repr(slot);

	if (ptr == TYPE_ERROR) return 0;
	TypeId base = type_canon(type_unconst_of(types[ptr].base));
	return base == TYPE_VOID || base == cls;
}

static void check_class_slot (Sema *s, uint32_t alloc, TypeId cls, uint32_t at)
{
	TypeId slot = s->ast->nodes[alloc].data_type;

	if (slot != TYPE_ERROR && type_ptr_repr(slot) != TYPE_ERROR
			&& !class_slot_ok(slot, cls))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + at),
				"the placement slot of a class must be 'ptr %s' or 'ptr void', not %s",
				types[cls].name, type_display_name(slot));
}

static int check_new_dims (Sema *s, uint32_t dim, uint32_t dims)
{
	int ok = 1;

	for (uint32_t d = 0; d < dims; d++) {
		TypeId type = check_expr(s, dim);

		if (type != TYPE_ERROR && !is_integer_operand(type)) {
			error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + dim),
					"the size of a 'new' allocation must be an integer, not %s",
					type_display_name(type));
			ok = 0;
		} else if (type != TYPE_ERROR) {
			finalize_type(s, dim, type);
		}
		dim = s->ast->nodes[dim].next_bro;
	}
	return ok;
}

static TypeId new_result_type (Sema *s, uint32_t idx, TypeId base, uint32_t dims)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId type = base;

	if (!dims) {
		type = type_ptr_to(base);
	} else {
		for (uint32_t d = 0; d < dims && type != TYPE_ERROR; d++)
			type = type_slice_of(type);
	}
	if (type == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	n->data_type = type;
	return type;
}

TypeId check_new (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	int custom = (n->flags & NODE_FLAG_CUSTOM_ALLOC) != 0;
	uint32_t dims = (uint32_t)n->u64;
	uint32_t alloc = custom ? n->child : NO_NODE;
	uint32_t dim = n->child;

	if (custom) {
		check_allocator(s, dim);
		dim = s->ast->nodes[dim].next_bro;
	} else {
		require_stdlib(s, n, "new");
	}

	n = s->ast->nodes + idx;
	TypeId base = resolve_type_node(s, n);
	n = s->ast->nodes + idx;
	if (base == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (!types[base].size || type_dyn_rank(base) > 0) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot allocate a value of type %s with 'new'",
				type_display_name(base));
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	if (custom && !dims && type_kind(type_canon(base)) == TK_CLASS)
		check_class_slot(s, alloc, type_canon(base), idx);
	if (!check_new_dims(s, dim, dims)) {
		s->ast->nodes[idx].data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	return new_result_type(s, idx, base, dims);
}

static void check_new_ctor_args (Sema *s, uint32_t idx, TypeId cls, uint32_t init, uint32_t argc)
{
	if (init != NO_NODE) {
		ASTNode call = {0};

		call.type = NODE_FN_CALL;
		call.child = s->ast->nodes[idx].child;
		call.str = (char *)types[cls].name;
		call.len = types[cls].len;
		call.sym = NO_SYMBOL;
		call.type_node = NO_TYPE_NODE;
		call.line = s->ast->nodes[idx].line;
		call.col = s->ast->nodes[idx].col;
		uint32_t call_idx = sema_new_node(s, call);

		check_ctor_args(s, call_idx, init);
		s->ast->nodes[idx].child = s->ast->nodes[call_idx].child;
	} else if (argc) {
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + idx),
				"class '%s' has no initializer; 'new' cannot take arguments",
				types[cls].name);
	}
}

static TypeId check_ctor_class (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId base = resolve_type_node(s, n);

	n = s->ast->nodes + idx;
	if (base == TYPE_ERROR) {
		n->data_type = TYPE_ERROR;
		return TYPE_ERROR;
	}
	TypeId cls = type_canon(base);

	if (type_kind(cls) == TK_CLASS) return cls;
	error_report(s->err, ERR_ERROR, node_loc(n),
			"constructor syntax requires a class type, not %s",
			type_display_name(base));
	n->data_type = TYPE_ERROR;
	return TYPE_ERROR;
}

TypeId check_new_ctor (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	int custom = (n->flags & NODE_FLAG_CUSTOM_ALLOC) != 0;
	uint32_t alloc = custom ? (uint32_t)n->u64 : NO_NODE;
	uint32_t argc = 0;

	for (uint32_t a = n->child; a != NO_NODE; a = s->ast->nodes[a].next_bro)
		argc++;

	TypeId cls = check_ctor_class(s, idx);

	if (cls == TYPE_ERROR) return TYPE_ERROR;
	n = s->ast->nodes + idx;
	if (custom) {
		check_allocator(s, alloc);
		n = s->ast->nodes + idx;
		check_class_slot(s, alloc, cls, idx);
	} else {
		require_stdlib(s, n, "new");
	}

	uint32_t init = class_init_node(s, cls);

	check_new_ctor_args(s, idx, cls, init, argc);
	n = s->ast->nodes + idx;
	n->sym = init == NO_NODE ? NO_SYMBOL : s->ast->nodes[init].sym;
	n->data_type = type_ptr_to(cls);
	if (n->data_type == TYPE_ERROR) {
		error_report(s->err, ERR_ERROR, node_loc(n), "too many types");
		return TYPE_ERROR;
	}
	return n->data_type;
}

void mark_placement (Sema *s, uint32_t sym, uint32_t init)
{
	if (init == NO_NODE) return;

	ASTNode *n = s->ast->nodes + init;

	while (n->type == NODE_CAST && n->child != NO_NODE)
		n = s->ast->nodes + n->child;
	if ((n->type == NODE_NEW || n->type == NODE_NEW_CTOR)
			&& (n->flags & NODE_FLAG_CUSTOM_ALLOC))
		s->table.symbols[sym].placement = 1;
}

static void check_deallocator (Sema *s, TypeId d, TypeId target, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	TypeId ptr = d == TYPE_ERROR ? TYPE_ERROR : type_ptr_repr(d);

	if (ptr == TYPE_ERROR || type_kind(types[ptr].base) != TK_FN) {
		if (d != TYPE_ERROR)
			error_report(s->err, ERR_ERROR, node_loc(n),
					"the deallocator expression of 'delete' must be a function pointer");
		return;
	}

	const Type *fn = types + types[ptr].base;
	if (fn->fn_variadic || fn->param_count != 1) {
		error_report(s->err, ERR_ERROR, node_loc(n),
				"the deallocator must take exactly one pointer argument");
		return;
	}
	if (type_kind(fn->params[0]) != TK_PTR)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"the deallocator parameter must be a pointer, not %s",
				type_display_name(fn->params[0]));
	else if (target != TYPE_ERROR && target != TYPE_NULL
			&& !sema_implicit_cast_ok(target, fn->params[0]))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"cannot pass a value of type %s to the deallocator parameter of type %s",
				type_display_name(target), type_display_name(fn->params[0]));
	if (fn->ret_count > 1)
		error_report(s->err, ERR_ERROR, node_loc(n),
				"the deallocator return type is not supported; use a scalar or void");
	else if (fn->ret_count == 1 && (type_is_struct_like(fn->rets[0])
			|| type_kind(fn->rets[0]) == TK_SLICE || type_kind(fn->rets[0]) == TK_ARRAY
			|| type_dyn_rank(fn->rets[0]) > 0))
		error_report(s->err, ERR_ERROR, node_loc(n),
				"the deallocator return type is not supported; use a scalar or void");
}

static int placement_target (Sema *s, uint32_t target)
{
	ASTNode *n = s->ast->nodes + target;

	while (n->type == NODE_CAST && n->child != NO_NODE)
		n = s->ast->nodes + n->child;
	if ((n->type == NODE_NEW || n->type == NODE_NEW_CTOR)
			&& (n->flags & NODE_FLAG_CUSTOM_ALLOC))
		return 1;
	return n->type == NODE_ID && n->sym != NO_SYMBOL
			&& s->table.symbols[n->sym].placement;
}

static uint32_t delete_destroy_sym (Sema *s, TypeId type)
{
	TypeId ptr = type_ptr_repr(type);

	if (ptr == TYPE_ERROR) return NO_SYMBOL;
	TypeId cls = type_canon(type_unconst_of(types[ptr].base));

	if (type_kind(cls) != TK_CLASS) return NO_SYMBOL;
	uint32_t destroy = class_destroy_node(s, cls);

	return destroy == NO_NODE ? NO_SYMBOL : s->ast->nodes[destroy].sym;
}

void check_delete (Sema *s, uint32_t idx)
{
	ASTNode *n = s->ast->nodes + idx;
	int custom = (n->flags & NODE_FLAG_CUSTOM_ALLOC) != 0;
	uint32_t target = custom ? s->ast->nodes[n->child].next_bro : n->child;
	TypeId dealloc = TYPE_ERROR;

	if (custom) {
		dealloc = check_expr(s, n->child);
	} else {
		require_stdlib(s, n, "delete");
	}
	n = s->ast->nodes + idx;
	TypeId type = check_expr(s, target);
	n = s->ast->nodes + idx;

	if (custom) check_deallocator(s, dealloc, type, idx);
	if (type != TYPE_ERROR && type != TYPE_NULL
			&& type_ptr_repr(type) == TYPE_ERROR && type_kind(type) != TK_SLICE)
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + target),
				"'delete' requires a pointer or a slice, not %s",
				type_display_name(type));
	else if (!custom && placement_target(s, target))
		error_report(s->err, ERR_ERROR, node_loc(s->ast->nodes + target),
				"memory from a custom allocator must be freed with"
				" 'delete(deallocator) pointer'");
	if (type != TYPE_ERROR && type != TYPE_NULL
			&& sema_borrowed_target(s, target, "delete")) {
		n->data_type = TYPE_ERROR;
		return;
	}
	n->sym = type == TYPE_ERROR ? NO_SYMBOL : delete_destroy_sym(s, type);
	n->data_type = TYPE_VOID;
}
