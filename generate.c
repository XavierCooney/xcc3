#include "xcc.h"


// TODO: make this not use global variables
static FILE *output_stream = NULL;
static bool has_begun_current_line = false;

int unique_label_num = 0;

#define OUT_STREAM output_stream

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
    xcc_assert(line);
    possibly_generate_indent();
    fprintf(OUT_STREAM, "%s", line);
}

void generate_asm_integer(long long val) {
    possibly_generate_indent();
    fprintf(OUT_STREAM, "%lld", val);
}

void generate_set_output(FILE *stream) {
    output_stream = stream;
}

void generate_asm(const char *line) {
    xcc_assert(output_stream);

    possibly_generate_indent();
    fprintf(OUT_STREAM, "%s", line);
    generate_end_of_line();
}

int get_unique_label_num(void) {
    return unique_label_num++;
}