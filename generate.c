#include "xcc.h"

#define OUT_STREAM stdout

// TODO: make this not use global variables

bool has_begun_current_line = false;

static void possibly_generate_indent() {
    if(!has_begun_current_line) {
        fprintf(OUT_STREAM, "    ");
        has_begun_current_line = true;
    }
}

static void generate_end_of_line() {
    fprintf(OUT_STREAM, "\n");
    has_begun_current_line = false;
}

void generate_asm_no_indent() {
    xcc_assert(!has_begun_current_line);
    has_begun_current_line = true;
}

void generate_asm_partial(const char *line) {
    possibly_generate_indent();
    fprintf(OUT_STREAM, "%s", line);
}

void generate_asm_integer(long long val) {
    possibly_generate_indent();
    fprintf(OUT_STREAM, "%lld", val);
}

void generate_asm(const char *line) {
    possibly_generate_indent();
    fprintf(OUT_STREAM, "%s", line);
    generate_end_of_line();
}