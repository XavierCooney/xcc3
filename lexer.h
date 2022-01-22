typedef enum {
    TOK_UNKNOWN,

    TOK_KEYWORD_INT,
    TOK_KEYWORD_RETURN,

    TOK_IDENTIFIER,

    TOK_OPEN_PAREN,
    TOK_CLOSE_PAREN,
    TOK_OPEN_CURLY,
    TOK_CLOSE_CURLY,
    TOK_SEMICOLON,

    TOK_INT_LITERAL,
}  TokenType;

typedef struct {
    TokenType type;

    const char *contents;
    size_t contents_length;

    int source_line_num;
    int source_column_num;
    size_t source_length;
} Token;

typedef struct {
    size_t num_tokens;
    size_t num_tokens_allocated;
    Token *tokens;

    const char *source;
    int source_length;
    int index;

    int current_line_num;
    int current_col_num;
} Lexer;


void lex_free_lexer(Lexer *lexer);
void lex_dump_token(Token *token);
void lex_dump_lexer_state(Lexer *lexer);
Lexer *lex_file(FILE *stream);
