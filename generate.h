#pragma once

#include "xcc.h"

void generate_asm_no_indent(void);
void generate_asm_partial(const char *line);
void generate_asm_integer(long long val);
int get_unique_label_num(void);
void generate_asm(const char *line);
void generate_set_output(FILE *stream);
void generate_x64(AST *ast, const char *filename);
