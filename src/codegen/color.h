#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

#include "arena/arena.h"
#include "codegen/regalloc.h"
#include "ir/cfg.h"
#include "ir/ir.h"

void color_regs (Arena *arena, IR *ir, IRFn *fn, Reg *regs, RegCtx *ctx, Cfg *cfg, uint32_t reg_num);

#endif
