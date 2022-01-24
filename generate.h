#pragma once

#include "xcc.h"

void generate_asm_no_indent();
void generate_asm_partial(const char *line);
void generate_asm_integer(long long val);
void generate_asm(const char *line);
void generate_x64(AST *ast, const char *filename);