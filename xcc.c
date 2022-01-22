#include <stdio.h>
#include "xcc.h"

static int number_xcc_allocations = 0;

__attribute__((__noreturn__)) void
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
    ++number_xcc_allocations;

    if(size == 0) {
        return NULL;
    }

    void *result = malloc(size);
    xcc_assert_msg(result, "malloc() returned NULL");
    return result;
}

void xcc_free(const void *p) {
    if(number_xcc_allocations < 1) {
        xcc_assert_msg(false, "double free?");
    }
    --number_xcc_allocations;

    if(p) {
        free((void *) p);
    }
}

int main(int argc, char **argv) {
    FILE *stream = fopen("test.c", "r");
    if(!stream) {
        perror("open()");
        return 1;
    }

    Lexer *lexer = lex_file(stream);
    if(fclose(stream)) {
        perror("close()");
        return 1;
    }

    lex_dump_lexer_state(lexer);
    lex_free_lexer(lexer);
}
