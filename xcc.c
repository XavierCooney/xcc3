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
        compiler_line_num, compiler_func_name, compiler_file_name
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

static bool is_verbose = false;
bool xcc_verbose() {
    return is_verbose;
}


int main(int argc, char **argv) {
    const char *filename_in = NULL;
    const char *filename_out = NULL;

    for(int i = 1; i < argc; ++i) {
        if(!strcmp(argv[i], "-o")) {
            if(i + 1 >= argc) {
                fprintf(stderr, "No output specified after `-o`\n");
                return 1;
            } else if(filename_out) {
                fprintf(stderr, "Two output files specified!");
                return 1;
            } else {
                filename_out = argv[i + 1];
                ++i;
            }
        } else if(!strcmp(argv[i], "-v")) {
            is_verbose = true;
        } else if(argv[i][0] != '-') {
            if(filename_in) {
                fprintf(stderr, "Two input files specified!");
                return 1;
            } else {
                filename_in = argv[i];
            }
        } else {
            fprintf(stderr, "Unknown argument `%s`\n", argv[i]);
            return 1;
        }
    }

    if(!filename_in) {
        fprintf(stderr, "No input file specified\n");
        return 1;
    }

    if(!filename_out) {
        fprintf(stderr, "No output file specified\n");
        return 1;
    }

    FILE *input_stream = fopen(filename_in, "r");
    if(!input_stream) {
        perror("open(input_stream)");
        return 1;
    }

    Lexer *lexer = lex_file(input_stream, filename_in);
    if(xcc_verbose()) lex_dump_lexer_state(lexer);

    if(fclose(input_stream)) {
        perror("close(input_stream)");
        return 1;
    }

    AST *program_ast = parse_program(lexer);
    if(xcc_verbose()) ast_dump(program_ast, "parsed");

    Resolutions *resolutions = resolve(program_ast);
    if(xcc_verbose()) ast_dump(program_ast, "resolved");

    check_lvalue(program_ast);

    type_propogate(program_ast);
    if(xcc_verbose()) ast_dump(program_ast, "typed");

    value_pos_allocate(program_ast);
    if(xcc_verbose()) ast_dump(program_ast, "allocated");

    FILE *output_stream = fopen(filename_out, "w");
    if(!output_stream) {
        perror("open(output_stream)");
        return 1;
    }

    generate_set_output(output_stream);
    generate_x64(program_ast, filename_in);

    if(fclose(output_stream)) {
        perror("close(output_stream)");
        return 1;
    }

    value_pos_free_preallocated();
    type_free_all();
    resolve_free(resolutions);
    ast_free(program_ast);
    lex_free_lexer(lexer);

    xcc_assert(!has_begun_prog_error);
    xcc_assert_msg(number_xcc_allocations == 0, "Memory leak!");
}
