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

static Token *prev_token(Parser *parser) {
    xcc_assert(parser->current_token > 0);
    return &parser->lexer->tokens[parser->current_token - 1];
}

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
    xcc_assert_not_reached();
}

static AST *accept_type(Parser *parser) {
    // returns NULL if no type is accepted

    // There's probably a better way of representing types than having a different
    // AST node for each type
    if (accept(parser, TOK_KEYWORD_INT)) {
        AST *type_ast = ast_new(AST_TYPE, prev_token(parser));
        ast_append_new(type_ast, AST_TYPE_INT, prev_token(parser));
        return type_ast;
    } else if (accept(parser, TOK_KEYWORD_CHAR)) {
        AST *type_ast = ast_new(AST_TYPE, prev_token(parser));
        ast_append_new(type_ast, AST_TYPE_CHAR, prev_token(parser));
        return type_ast;
    } else if (accept(parser, TOK_KEYWORD_VOID)) {
        AST *type_ast = ast_new(AST_TYPE, prev_token(parser));
        ast_append_new(type_ast, AST_TYPE_VOID, prev_token(parser));
        return type_ast;
    }

    return NULL;
}

static AST *expect_type(Parser *parser) {
    AST *type = accept_type(parser);
    if(type) return type;

    parse_error(parser, "expected type");
}

static long long parse_integer_literal_value(Token *token) {
    errno = 0;
    long long val = strtoll(token->contents, NULL, 10);

    if(errno == ERANGE) {
        prog_error("integer literal out of range", token);
    }

    return val;
}

static AST *parse_expression(Parser *parser);
static AST *parse_statement(Parser *parser);

static AST *parse_primary(Parser *parser) {
    if(accept(parser, TOK_INT_LITERAL)) {
        Token *literal_token = prev_token(parser);
        AST *literal_ast = ast_new(AST_INTEGER_LITERAL, literal_token);
        literal_ast->integer_literal_val = parse_integer_literal_value(literal_token);
        return literal_ast;
    } else if(accept(parser, TOK_IDENTIFIER)) {
        Token *ident_token = prev_token(parser);

        if(accept(parser, TOK_OPEN_PAREN)) {
            AST *ast_call = ast_new(AST_CALL, ident_token);
            ast_call->identifier_string = ident_token->contents;

            while(!accept(parser, TOK_CLOSE_PAREN)) {
                ast_append(ast_call, parse_expression(parser));
                if(current_token(parser)->type != TOK_CLOSE_PAREN) {
                    expect(parser, TOK_COMMA);
                }
            }

            return ast_call;
        } else {
            AST *var_usage_ast = ast_new(AST_VAR_USE, ident_token);
            var_usage_ast->identifier_string = ident_token->contents;
            return var_usage_ast;
        }
    } else {
        parse_error(parser, "need expression");
    }
}

static AST *parse_multiplicative(Parser *parser) {
    AST *a = parse_primary(parser);

    return a;
}

static AST *parse_additive(Parser *parser) {
    AST *a = parse_multiplicative(parser);

    while(accept(parser, TOK_PLUS) || accept(parser, TOK_MINUS)) {
        Token *token = prev_token(parser);

        AST *b = parse_multiplicative(parser);

        ASTType ast_type = token->type == TOK_PLUS ? AST_ADD : AST_SUBTRACT;
        AST *add_ast = ast_new(ast_type, token);
        ast_append(add_ast, a);
        ast_append(add_ast, b);
        a = add_ast;
    }

    return a;
}

static AST *parse_assignment(Parser *parser) {
    // technically this is not to-spec but in that case it's probably not an lvalue anyway
    AST *a = parse_additive(parser);

    if(accept(parser, TOK_EQUALS)) {
        AST *assignment_ast = ast_new(AST_ASSIGN, prev_token(parser));
        // The call to parse_assignment rather than parse_additive
        // here allows assignment to be right associative
        AST *b = parse_assignment(parser);

        ast_append(assignment_ast, a);
        ast_append(assignment_ast, b);

        a = assignment_ast;
    }

    return a;
}

static AST *parse_expression(Parser *parser) {
    return parse_assignment(parser);
}

static AST *parse_if(Parser *parser) {
    AST *if_ast = ast_new(AST_IF, prev_token(parser));

    expect(parser, TOK_OPEN_PAREN);
    AST *condition_expression = parse_expression(parser);
    ast_append(if_ast, condition_expression);
    expect(parser, TOK_CLOSE_PAREN);

    AST *statement = parse_statement(parser);
    ast_append(if_ast, statement);

    return if_ast;
}

static AST *parse_statement(Parser *parser) {
    Token *first_token = current_token(parser);
    AST *ast_type;

    if (accept(parser, TOK_KEYWORD_RETURN)) {
        AST *return_ast = ast_new(AST_RETURN_STMT, first_token);

        if (!accept(parser, TOK_SEMICOLON)) {
            ast_append(return_ast, parse_expression(parser));
            expect(parser, TOK_SEMICOLON);
        }
        return return_ast;
    } else if (accept(parser, TOK_KEYWORD_IF)) {
        return parse_if(parser);
    } else if ((ast_type = accept_type(parser))) {
        // TODO: actually properly parse declarations...
        Token *var_name_token = expect(parser, TOK_IDENTIFIER);
        const char *var_name = var_name_token->contents;

        AST *var_declare_ast = ast_new(AST_VAR_DECLARE, var_name_token);
        var_declare_ast->identifier_string = var_name;
        ast_append(var_declare_ast, ast_type);

        if(accept(parser, TOK_EQUALS)) {
            AST *initialiser = parse_expression(parser);
            ast_append(var_declare_ast, initialiser);
        }
        expect(parser, TOK_SEMICOLON);

        return var_declare_ast;
    } else {
        AST *expression_ast = parse_expression(parser);
        AST *statement_ast = ast_new(AST_STATEMENT_EXPRESSION, expression_ast->main_token);
        ast_append(statement_ast, expression_ast);
        expect(parser, TOK_SEMICOLON);
        return statement_ast;
    }
}

static AST *parse_block(Parser *parser) {
    AST *body_ast = ast_new(AST_BODY, current_token(parser));
    expect(parser, TOK_OPEN_CURLY);

    while (!accept(parser, TOK_CLOSE_CURLY)) {
        AST *statement = parse_statement(parser);
        ast_append(body_ast, statement);
    }

    return body_ast;
}

static AST *parse_function(Parser *parser) {
    AST *return_type_ast = expect_type(parser);

    Token *func_name_token = expect(parser, TOK_IDENTIFIER);

    AST *param_list = ast_new(AST_FUNC_DECL_PARAM_LIST, current_token(parser));
    expect(parser, TOK_OPEN_PAREN);

    while (!accept(parser, TOK_CLOSE_PAREN)) {
        AST *param_type = expect_type(parser);
        Token *param_name_token = accept(parser, TOK_IDENTIFIER);
        AST *param = ast_new(AST_PARAMETER, param_name_token);
        param->identifier_string = param_name_token->contents;
        ast_append(param, param_type);
        ast_append(param_list, param);

        if(current_token(parser)->type != TOK_CLOSE_PAREN) {
            expect(parser, TOK_COMMA);
        }
    }

    bool just_prototype = accept(parser, TOK_SEMICOLON);

    AST *func_ast = ast_new(
        just_prototype ? AST_FUNCTION_PROTOTYPE : AST_FUNCTION, func_name_token
    );
    func_ast->identifier_string = func_name_token->contents;
    ast_append(func_ast, return_type_ast);
    ast_append(func_ast, param_list);

    if(just_prototype) return func_ast;

    AST *block_ast = parse_block(parser);
    ast_append(func_ast, block_ast);

    if (!strcmp(func_name_token->contents, "main")) {
        // main() has implicit return 0
        AST *return_stmt = ast_append_new(block_ast, AST_RETURN_STMT, prev_token(parser));
        AST *literal_0 = ast_append_new(return_stmt, AST_INTEGER_LITERAL, prev_token(parser));
        literal_0->integer_literal_val = 0;
    }

    return func_ast;
}

static AST *parse_unit(Parser *parser) {
    AST *program_ast = ast_new(AST_PROGRAM, current_token(parser));

    while (current_token(parser)->type != TOK_EOF) {
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