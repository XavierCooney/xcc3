#include "xcc.h"


const char *lex_token_type_to_string(TokenType type) {
    switch(type) {
        case TOK_KEYWORD_INT: return "KEYWORD_INT";
        case TOK_KEYWORD_CHAR: return "KEYWORD_CHAR";
        case TOK_KEYWORD_RETURN: return "KEYWORD_RETURN";
        case TOK_IDENTIFIER: return "IDENTIFIER";
        case TOK_OPEN_PAREN: return "OPEN_PAREN";
        case TOK_CLOSE_PAREN: return "CLOSE_PAREN";
        case TOK_OPEN_CURLY: return "OPEN_CURLY";
        case TOK_SEMICOLON: return "SEMICOLON";
        case TOK_EQUALS: return "EQUALS";
        case TOK_COMMA: return "COMMA";
        case TOK_CLOSE_CURLY: return "CLOSE_CURLY";
        case TOK_INT_LITERAL: return "INT_LITERAL";
        case TOK_PLUS: return "PLUS";
        case TOK_EOF: return "EOF";
        case TOK_UNKNOWN: return "UNKNOWN";
    }

    return "INVALID";
    xcc_assert_not_reached();
}

void lex_dump_token(Token *token) {
    fprintf(stderr, "[TOKEN %s `", lex_token_type_to_string(token->type));
    fprintf(stderr, "%s", token->contents);
    fprintf(stderr, "` (id=%d, len=%zu, ", (int) token->type, token->contents_length);
    fprintf(stderr, "line=%d, col=%d]", token->source_line_num, token->source_column_num);
}

void lex_dump_lexer_state(Lexer *lexer) {
    fprintf(stderr, "\nLexer state:");
    fprintf(
        stderr, " line=%d, col=%d, idx=%d, num_tokens=%zu\n",
        lexer->current_line_num, lexer->current_col_num,
        lexer->index, lexer->num_tokens
    );
    for(int i = 0; i < lexer->num_tokens; ++i) {
        fprintf(stderr, "    ");
        lex_dump_token(&lexer->tokens[i]);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
}

void lex_print_source_with_token_range(Token *start, Token *end) {
    bool do_colour = isatty(fileno(stderr));

    // TODO: actually use `end`
    fprintf(
        stderr, "    At \"%s:%d:%d\": (",
        start->source_filename, start->source_line_num,
        start->source_column_num
    );
    lex_dump_token(start);
    fprintf(stderr, ")\n");

    xcc_assert(start->source_column_num - 1 + start->source_length <= strlen(start->start_of_line));

    fprintf(stderr, "    | ");
    for(int i = 0; i < start->source_column_num - 1; ++i) {
        fprintf(stderr, "%c", start->start_of_line[i]);
    }
    if(do_colour) {
        fprintf(stderr, "\033[31;1m");
    }
    for(int i = 0; i < start->source_length; ++i) {
        fprintf(
            stderr, "%c", start->start_of_line[i + start->source_column_num - 1]
        );
    }
    if(do_colour) {
        fprintf(stderr, "\033[0m");
    }
    for(int i = start->source_column_num - 1 + start->source_length; true; ++i) {
        char c = start->start_of_line[i];
        if(c == '\n' || c == '\0') {
            break;
        }
        fprintf(
            stderr, "%c", c
        );
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "      ");
    for(int i = 0; i < start->source_column_num - 1; ++i) {
        fprintf(stderr, " ");
    }
    if(do_colour) {
        fprintf(stderr, "\033[31;1m");
    }
    for(int i = 0; i < start->source_length; ++i) {
        char c = i ? '~' : '^';
        fprintf(stderr, "%c", c);
    }
    if(do_colour) {
        fprintf(stderr, "\033[0m");
    }
    fprintf(stderr, "\n");
}

static Token *append_empty_token(Lexer *lexer) {
    Token *new_token;
    LIST_STRUCT_APPEND_FUNC(
        Token, lexer, num_tokens, num_tokens_allocated,
        tokens, new_token
    );
    return new_token;
}

static void advance_one_char(Lexer *lexer) {
    if(lexer->index >= lexer->source_length) {
        xcc_assert_not_reached_msg("Lexer advancing past \\0");
    }

    char c = lexer->source[lexer->index];
    ++lexer->index;

    if(c == '\n') {
        ++lexer->current_line_num;
        lexer->current_start_of_line_char = &lexer->source[lexer->index];
        lexer->current_col_num = 1;
    } else {
        ++lexer->current_col_num;
    }
}

static Token *accept_token(Lexer *lexer, TokenType type, int tok_length) {
    Token *new_token = append_empty_token(lexer);

    char *new_contents = xcc_malloc(tok_length + 1);
    new_token->contents = new_contents;
    new_token->contents_length = tok_length;
    new_token->source_length = tok_length;
    new_token->source_column_num = lexer->current_col_num;
    new_token->source_line_num = lexer->current_line_num;
    new_token->start_of_line = lexer->current_start_of_line_char;
    new_token->source_filename = lexer->source_filename;
    new_token->type = type;

    for(int i = 0; i < tok_length; ++i) {
        new_contents[i] = lexer->source[lexer->index];
        advance_one_char(lexer);
    }
    new_contents[tok_length] = '\0';

    return new_token;
}

static void free_token(Token *token) {
    xcc_free(token->contents);
}

void lex_free_lexer(Lexer *lexer) {
    for(int i = 0; i < lexer->num_tokens; ++i) {
        free_token(&lexer->tokens[i]);
    }
    if(lexer->tokens) xcc_free(lexer->tokens);

    xcc_free(lexer->source);
    xcc_free(lexer->source_filename);

    xcc_free(lexer);
}

static bool try_lex_a_comment(Lexer *lexer) {
    // Check for `//`
    if(lexer->source[lexer->index] == '/' && lexer->source[lexer->index + 1] == '/') {
        advance_one_char(lexer);
        advance_one_char(lexer);
        while(lexer->source[lexer->index] && lexer->source[lexer->index] != '\n') {
            advance_one_char(lexer);
        }
        if(lexer->source[lexer->index] == '\n') {
            advance_one_char(lexer); // gobble up the newline because why not?
        }
        return true;
    }
    return false;
}

static Token *try_lex_a_char(Lexer *lexer, TokenType type, char c) {
    if(lexer->source[lexer->index] == c) {
        return accept_token(lexer, type, 1);
    }

    return NULL;
}

static bool char_can_be_in_ident(char c, int index) {
    bool condition = ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
    if(index > 0) {
        condition = condition || ('0' <= c && c <= '9');
    }
    return condition;
}

static Token *try_lex_an_identifier(Lexer *lexer) {
    int tok_length = 0;

    while(char_can_be_in_ident(lexer->source[lexer->index + tok_length], tok_length)) {
        ++tok_length;
    }

    if(tok_length > 0) {
        return accept_token(lexer, TOK_IDENTIFIER, tok_length);
    }

    return NULL;
}

static bool char_can_be_in_integer(char c) {
    return '0' <= c && c <= '9';
}

static Token *try_lex_an_integer(Lexer *lexer) {
    int tok_length = 0;

    while(char_can_be_in_integer(lexer->source[lexer->index + tok_length])) {
        ++tok_length;
    }

    if(tok_length > 0) {
        return accept_token(lexer, TOK_INT_LITERAL, tok_length);
    }

    return NULL;
}

static void match_keyword_token(Token *token, const char *match, TokenType type) {
    if(!strcmp(token->contents, match)) {
        token->type = type;
    }
}

static bool current_char_is_whitespace(Lexer *lexer) {
    char c = lexer->source[lexer->index];
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static void lex_a_token(Lexer *lexer) {
    Token *token;

    if(try_lex_a_char(lexer, TOK_OPEN_PAREN, '(')) {
        return;
    } else if(try_lex_a_char(lexer, TOK_CLOSE_PAREN, ')')) {
        return;
    } else if(try_lex_a_char(lexer, TOK_OPEN_CURLY, '{')) {
        return;
    } else if(try_lex_a_char(lexer, TOK_CLOSE_CURLY, '}')) {
        return;
    } else if(try_lex_a_char(lexer, TOK_SEMICOLON, ';')) {
        return;
    } else if(try_lex_a_char(lexer, TOK_COMMA, ',')) {
        return;
    } else if(try_lex_a_char(lexer, TOK_PLUS, '+')) {
        return;
    } else if(try_lex_a_char(lexer, TOK_EQUALS, '=')) {
        return;
    } else if(current_char_is_whitespace(lexer)) {
        advance_one_char(lexer);
        return;
    } else if((token = try_lex_an_identifier(lexer))) {
        match_keyword_token(token, "int", TOK_KEYWORD_INT);
        match_keyword_token(token, "char", TOK_KEYWORD_CHAR);
        match_keyword_token(token, "return", TOK_KEYWORD_RETURN);
        return;
    } else if(try_lex_an_integer(lexer)) {
        return;
    } else if(try_lex_a_comment(lexer)) {
        return;
    } else {
        // Just to prevent an infinite loop...
        accept_token(lexer, TOK_UNKNOWN, 1);
        return;
    }

    xcc_assert_not_reached();
}

static Lexer *lex_source(const char *source, const char *filename) {
    // Takes ownership of source
    Lexer *lexer = xcc_malloc(sizeof(Lexer));

    lexer->source = source;
    lexer->source_length = strlen(source);
    lexer->index = 0;

    lexer->current_col_num = 1;
    lexer->current_line_num = 1;
    lexer->current_start_of_line_char = lexer->source;

    char *source_filename_buf = xcc_malloc(strlen(filename) + 1); // TODO: xcc_strdup
    strcpy(source_filename_buf, filename);
    lexer->source_filename = source_filename_buf;

    lexer->num_tokens = 0;
    lexer->num_tokens_allocated = 0;
    lexer->tokens = NULL;

    while(lexer->index != lexer->source_length) {
        lex_a_token(lexer);
    }
    accept_token(lexer, TOK_EOF, 0);

    return lexer;
}

static const char *read_file(FILE *stream) {
    // Caller takes ownership of return value
    size_t buf_length = 1024;
    size_t source_length = 0;
    char *buf = xcc_malloc(buf_length);

    while(!feof(stream)) {
        int c = fgetc(stream);
        if(c == EOF) {
            xcc_assert_msg(!ferror(stream), "error reading file");
            break;
        }
        xcc_assert_msg(c, "null character in file");

        if(source_length + 1 >= buf_length) {
            size_t new_buf_length = buf_length * 2;
            char *new_buf = xcc_malloc(new_buf_length);
            memcpy(new_buf, buf, source_length);
            xcc_free(buf);

            buf_length = new_buf_length;
            buf = new_buf;
        }

        buf[source_length] = c;
        ++source_length;
    }

    xcc_assert(source_length < buf_length);
    buf[source_length] = '\0';

    return buf;
}

Lexer *lex_file(FILE *stream, const char *filename) {
    const char *source = read_file(stream);

    return lex_source(source, filename);
}
