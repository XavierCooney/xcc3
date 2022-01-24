#include "xcc.h"

typedef struct {
    Lexer *lexer;
    int current_token;
} Parser;

static Token *current_token(Parser *parser) {
    return &parser->lexer->tokens[parser->current_token];
}

NORETURN static void parse_error(Parser *parser, const char *msg) {
    begin_prog_error_range(msg, current_token(parser), current_token(parser));
    fprintf(
        stderr, "  (current token is %s)\n",
        lex_token_type_to_string(current_token(parser)->type)
    );
    end_prog_error();
}

static Token *advance(Parser *parser) {
    xcc_assert(parser->current_token + 1 < parser->lexer->num_tokens);
    Token *token = &parser->lexer->tokens[parser->current_token];
    parser->current_token++;
    return token;
}

// static Token *prev_token(Parser *parser) {
//     xcc_assert(parser->current_token > 0);
//     return &parser->lexer->tokens[parser->current_token - 1];
// }

static Token *accept(Parser *parser, TokenType token_type) {
    Token *current = current_token(parser);

    if(current->type == token_type) {
        advance(parser);
        return current;
    } else {
        return NULL;
    }
}

static Token *expect(Parser *parser, TokenType token_type) {
    Token *current = current_token(parser);

    if(current->type == token_type) {
        advance(parser);
        return current;
    } else {
        begin_prog_error_range(NULL, current_token(parser), current_token(parser));
        fprintf(
            stderr, "  Expected a %s, but found a %s\n",
            lex_token_type_to_string(token_type),
            lex_token_type_to_string(current_token(parser)->type)
        );
        end_prog_error();
    }
    xcc_assert_not_reached()
}

static AST *parse_type(Parser *parser) {
    AST *type_ast = ast_new(AST_TYPE, current_token(parser));
    expect(parser, TOK_KEYWORD_INT);
    return type_ast;
}

static long long parse_integer_literal_value(Token *token) {
    errno = 0;
    long long val = strtoll(token->contents, NULL, 10);

    if(errno == ERANGE) {
        prog_error("Integer literal out of range", token);
    }

    return val;
}

static AST *parse_expression(Parser *parser) {
    Token *first_token = current_token(parser);

    if(accept(parser, TOK_INT_LITERAL)) {
        AST *literal_ast = ast_new(AST_INTEGER_LITERAL, first_token);
        literal_ast->integer_literal_val = parse_integer_literal_value(first_token);
        return literal_ast;
    } else {
        parse_error(parser, "Need expression");
    }
}

static AST *parse_statement(Parser *parser) {
    Token *first_token = current_token(parser);
    if(accept(parser, TOK_KEYWORD_RETURN)) {
        AST *return_ast = ast_new(AST_RETURN_STMT, first_token);
        ast_append(return_ast, parse_expression(parser));
        expect(parser, TOK_SEMICOLON);
        return return_ast;
    } else {
        parse_error(parser, "Need statement");
    }
}

static AST *parse_block(Parser *parser) {
    AST *body_ast = ast_new(AST_BODY, current_token(parser));
    expect(parser, TOK_OPEN_CURLY);

    while(!accept(parser, TOK_CLOSE_CURLY)) {
        AST *statement = parse_statement(parser);
        ast_append(body_ast, statement);
    }

    return body_ast;
}

static AST *parse_function(Parser *parser) {
    AST *return_type_ast = parse_type(parser);

    Token *func_name_token = expect(parser, TOK_IDENTIFIER);
    AST *func_ast = ast_new(AST_FUNCTION, func_name_token);
    func_ast->identifier_string = func_name_token->contents;
    ast_append(func_ast, return_type_ast);

    ast_append_new(func_ast, AST_FUNC_DECL_PARAM_LIST, current_token(parser));
    expect(parser, TOK_OPEN_PAREN);
    expect(parser, TOK_CLOSE_PAREN);

    ast_append(
        func_ast, parse_block(parser)
    );

    return func_ast;
}

static AST *parse_unit(Parser *parser) {
    AST *program_ast = ast_new(AST_PROGRAM, current_token(parser));

    while(current_token(parser)->type != TOK_EOF) {
        AST *func_ast = parse_function(parser);
        ast_append(program_ast, func_ast);
    }

    return program_ast;
}

AST *parse_program(Lexer *lexer) {
    Parser parser;
    parser.lexer = lexer;
    parser.current_token = 0;

    const char *old_stage = xcc_get_prog_error_stage();
    xcc_set_prog_error_stage("Parse");
    AST *program_ast = parse_unit(&parser);
    xcc_set_prog_error_stage(old_stage);


    return program_ast;
}