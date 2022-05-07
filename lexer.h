#pragma once
#include "xcc.h"

typedef enum {
    TOK_UNKNOWN,
    TOK_EOF,

    TOK_KEYWORD_INT,
    TOK_KEYWORD_CHAR,
    TOK_KEYWORD_VOID,
    TOK_KEYWORD_RETURN,
    TOK_KEYWORD_IF,
    TOK_KEYWORD_ELSE,
    TOK_KEYWORD_WHILE,

    TOK_IDENTIFIER,

    TOK_OPEN_PAREN,
    TOK_CLOSE_PAREN,
    TOK_OPEN_CURLY,
    TOK_CLOSE_CURLY,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_EQUALS,

    TOK_INT_LITERAL,

    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,

    TOK_LT, TOK_LT_OR_EQ, TOK_GT, TOK_GT_OR_EQ
} TokenType;

typedef struct Token {
    TokenType type;

    const char *contents;
    size_t contents_length;

    int source_line_num;
    int source_column_num;
    size_t source_length;

    const char *start_of_line;
    const char *source_filename;

    struct Token *alternative_source_token;
} Token;

typedef struct PreprocessorMacro {
    const char *name;
    Token *contents;
    int number_tokens;

    struct PreprocessorMacro *next_macro;
} PreprocessorMacro;

typedef struct {
    size_t num_tokens;
    size_t num_tokens_allocated;
    Token *tokens;

    const char *source;
    int source_length;
    int index;

    int current_line_num;
    int current_col_num;
    bool seen_nonwhitespace_on_line;
    const char *current_start_of_line_char;
    const char *source_filename;

    PreprocessorMacro *macros;
} Lexer;

const char *lex_token_type_to_string(TokenType type);
void lex_free_lexer(Lexer *lexer);
void lex_dump_token(Token *token);
void lex_print_source_with_token_range(Token *start, Token *end);
void lex_dump_lexer_state(Lexer *lexer);
Lexer *lex_file(FILE *stream, const char *filename);
