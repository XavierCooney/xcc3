#include <stdio.h>
#include "xcc.h"

static int number_xcc_allocations = 0;

NORETURN void
xcc_assert_error_internal(const char *msg, int compiler_line_num,
                          const char* compiler_file_name,
                          const char *compiler_func_name) {
    if(msg) {
        fprintf(stderr, "Compiler assertion failure: %s\n", msg);
    } else {
        fprintf(stderr, "Compiler assertion failure\n");
    }

    fprintf(
        stderr,
        "    (on compiler line %d in function %s from file %s)\n",
        compiler_line_num, compiler_file_name, compiler_func_name
    );

    abort(); // Raise SIGABRT to trigger GDB
}

void *xcc_malloc(size_t size) {
    if(size == 0) {
        return NULL;
    }

    ++number_xcc_allocations;

    void *result = malloc(size);
    xcc_assert_msg(result, "malloc() returned NULL");
    return result;
}

void xcc_free(const void *p) {
    if(!p) return;

    if(number_xcc_allocations < 1) {
        if(!getenv("RUNNING_IN_VALGRIND")) {
            xcc_assert_msg(false, "double free?");
        }
    }
    --number_xcc_allocations;

    free((void *) p);
}

static bool has_begun_prog_error = false;
static const char *current_compiling_stage_error_msg = NULL;

const char *xcc_get_prog_error_stage() {
    return current_compiling_stage_error_msg;;
}

void xcc_set_prog_error_stage(const char *stage) {
    current_compiling_stage_error_msg = stage;
}

void begin_prog_error_range(const char *msg, Token *start_token, Token *end_token) {
    const char *stage = current_compiling_stage_error_msg ? current_compiling_stage_error_msg : "Program";
    if(msg) {
        fprintf( stderr, "%s error: %s:\n", stage, msg);
    } else {
        fprintf( stderr, "%s error!\n", stage);
    }

    lex_print_source_with_token_range(start_token, end_token);
    has_begun_prog_error = true;
}

NORETURN void end_prog_error() {
    xcc_assert_msg(has_begun_prog_error, "end_prog_error error without begin");
    fprintf(stderr, " === end program error ===\n");
    abort();
}


int main(int argc, char **argv) {
    const char *filename = "test.c";

    FILE *stream = fopen(filename, "r");
    if(!stream) {
        perror("open()");
        return 1;
    }

    Lexer *lexer = lex_file(stream, filename);
    if(fclose(stream)) {
        perror("close()");
        return 1;
    }

    lex_dump_lexer_state(lexer);
    AST *program_ast = parse_program(lexer);
    ast_dump(program_ast);
    generate_x64(program_ast, filename);

    ast_free(program_ast);

    lex_free_lexer(lexer);
    xcc_assert(!has_begun_prog_error);
    xcc_assert_msg(number_xcc_allocations == 0, "Memory leak!");
}
