#include "xcc.h"

typedef struct {
    Lexer *lexer;
    int current_token;
} Parser;

static Token *current_token(Parser *parser) {
    return &parser->lexer->tokens[parser->current_token];
}

static Token *prev_token(Parser *parser) {
    xcc_assert(parser->current_token > 0);
    return &parser->lexer->tokens[parser->current_token - 1];
}

NORETURN static void parse_error(Parser *parser, const char *msg) {
    begin_prog_error_range(msg, current_token(parser), current_token(parser));
    fprintf(
        stderr, "  (current token is %s)\n",
        lex_token_type_to_string(current_token(parser)->type)
    );
    end_prog_error();
}

NORETURN static void parse_error_prev(Parser *parser, const char *msg) {
    begin_prog_error_range(msg, prev_token(parser), prev_token(parser));
    fprintf(
        stderr, "  (current token is %s)\n",
        lex_token_type_to_string(prev_token(parser)->type)
    );
    end_prog_error();
}

static Token *advance(Parser *parser) {
    xcc_assert(parser->current_token + 1 < parser->lexer->num_tokens);
    Token *token = &parser->lexer->tokens[parser->current_token];
    parser->current_token++;
    return token;
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

AST *expect_type(Parser *parser) {
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
static AST *parse_declaration(Parser *parser, bool as_parameter);
static bool current_token_is_specifier(Parser *parser);
static AST *parse_statement(Parser *parser);
static AST *parse_block(Parser *parser);

static AST *parse_primary(Parser *parser) {
    if(accept(parser, TOK_OPEN_PAREN)) {
        AST *expr = parse_expression(parser);
        expect(parser, TOK_CLOSE_PAREN);
        return expr;
    } else if(accept(parser, TOK_INT_LITERAL)) {
        Token *literal_token = prev_token(parser);
        AST *literal_ast = ast_new(AST_INTEGER_LITERAL, literal_token);
        literal_ast->integer_literal_val = parse_integer_literal_value(literal_token);
        return literal_ast;
    } else if(accept(parser, TOK_IDENTIFIER)) {
        Token *ident_token = prev_token(parser);

        AST *ident_usage_ast = ast_new(AST_IDENT_USE, ident_token);
        ident_usage_ast->identifier_string = ident_token->contents;
        return ident_usage_ast;
    } else {
        parse_error(parser, "need expression");
    }
}

static AST *parse_unary_postfix(Parser *parser) {
    AST *a = parse_primary(parser);

    while (accept(parser, TOK_OPEN_PAREN)) {
        AST *ast_call = ast_new(AST_CALL, a->main_token);
        ast_append(ast_call, a);

        while(!accept(parser, TOK_CLOSE_PAREN)) {
            ast_append(ast_call, parse_expression(parser));
            if(current_token(parser)->type != TOK_CLOSE_PAREN) {
                expect(parser, TOK_COMMA);
            }
        }

        a = ast_call;
    }

    return a;
}

static AST *parse_multiplicative(Parser *parser) {
    AST *a = parse_unary_postfix(parser);

    while (accept(parser, TOK_STAR) || accept(parser, TOK_SLASH)) {
        Token *token = prev_token(parser);

        AST *b = parse_unary_postfix(parser);

        ASTType ast_type = token->type == TOK_STAR ? AST_MULTIPLY : AST_DIVIDE;
        AST *add_ast = ast_new(ast_type, token);
        ast_append(add_ast, a);
        ast_append(add_ast, b);
        a = add_ast;
    }

    return a;
}

static AST *parse_additive(Parser *parser) {
    AST *a = parse_multiplicative(parser);

    while (accept(parser, TOK_PLUS) || accept(parser, TOK_MINUS)) {
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

static AST *parse_comparison(Parser *parser) {
    AST *a = parse_additive(parser);

    int number_chained = 0;

    while (accept(parser, TOK_LT) || accept(parser, TOK_GT) ||
            accept(parser, TOK_LT_OR_EQ) || accept(parser, TOK_GT_OR_EQ)) {

        if (number_chained >= 1) {
            // TODO: this should technically be a warning, but I don't have
            // the code for that yet, so I'll do an error
            parse_error(parser, "sus chaining of comparison operators");
        }

        Token *comparison_token = prev_token(parser);
        TokenType token_type = comparison_token->type;
        ASTType ast_type;

        if (token_type == TOK_LT) {
            ast_type = AST_CMP_LT;
        } else if (token_type == TOK_GT) {
            ast_type = AST_CMP_GT;
        } else if (token_type == TOK_LT_OR_EQ) {
            ast_type = AST_CMP_LT_EQ;
        } else if (token_type == TOK_GT_OR_EQ) {
            ast_type = AST_CMP_GT_EQ;
        } else {
            xcc_assert_not_reached();
        }

        AST *comparison_ast = ast_new(ast_type, comparison_token);

        AST *b = parse_additive(parser);

        ast_append(comparison_ast, a);
        ast_append(comparison_ast, b);

        a = comparison_ast;

        number_chained++;
    }

    return a;
}

static AST *parse_assignment(Parser *parser) {
    // technically this is not to-spec but in that case it's probably not an lvalue anyway
    // this is also apparently how many other compilers work
    AST *a = parse_comparison(parser);

    if (accept(parser, TOK_EQUALS)) {
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

    if (accept(parser, TOK_KEYWORD_ELSE)) {
        ast_append(if_ast, parse_statement(parser));
    }

    return if_ast;
}

static AST *parse_while(Parser *parser) {
    AST *while_ast = ast_new(AST_WHILE, prev_token(parser));

    expect(parser, TOK_OPEN_PAREN);
    AST *condition_expression = parse_expression(parser);
    ast_append(while_ast, condition_expression);
    expect(parser, TOK_CLOSE_PAREN);

    AST *statement = parse_statement(parser);
    ast_append(while_ast, statement);

    return while_ast;
}

static AST *parse_statement(Parser *parser) {
    if (accept(parser, TOK_KEYWORD_RETURN)) {
        AST *return_ast = ast_new(AST_RETURN_STMT, prev_token(parser));

        if (!accept(parser, TOK_SEMICOLON)) {
            ast_append(return_ast, parse_expression(parser));
            expect(parser, TOK_SEMICOLON);
        }
        return return_ast;
    } else if (accept(parser, TOK_KEYWORD_IF)) {
        return parse_if(parser);
    } else if (accept(parser, TOK_KEYWORD_WHILE)) {
        return parse_while(parser);
    } else if (current_token_is_specifier(parser)) {
        return parse_declaration(parser, false);
    } else if (accept(parser, TOK_OPEN_CURLY)) {
        return parse_block(parser);
    } else {
        AST *expression_ast = parse_expression(parser);
        AST *statement_ast = ast_new(AST_STATEMENT_EXPRESSION, expression_ast->main_token);
        ast_append(statement_ast, expression_ast);
        expect(parser, TOK_SEMICOLON);
        return statement_ast;
    }
}

static AST *parse_block(Parser *parser) {
    AST *body_ast = ast_new(AST_BLOCK_STATEMENT, prev_token(parser));

    while (!accept(parser, TOK_CLOSE_CURLY)) {
        AST *statement = parse_statement(parser);
        ast_append(body_ast, statement);
    }

    return body_ast;
}

static bool current_token_is_specifier(Parser *parser) {
    TokenType current_type = current_token(parser)->type;
    switch (current_type) {
        case TOK_KEYWORD_INT: return true;
        case TOK_KEYWORD_CHAR: return true;
        case TOK_KEYWORD_VOID: return true;
        case TOK_KEYWORD_SHORT: return true;
        case TOK_KEYWORD_LONG: return true;
        case TOK_KEYWORD_SIGNED: return true;
        case TOK_KEYWORD_UNSIGNED: return true;
        default: return false;
    }
    xcc_assert_not_reached();
}

static AST *parse_declarator(Parser *parser) {
    AST *declarator = NULL;

    if (accept(parser, TOK_OPEN_PAREN)) {
        declarator = parse_declarator(parser);
        expect(parser, TOK_CLOSE_PAREN);
    } else if (accept(parser, TOK_IDENTIFIER)) {
        declarator = ast_new(AST_DECLARATOR_IDENT, prev_token(parser));
        declarator->identifier_string = declarator->main_token->contents;
    } else {
        parse_error(parser, "expected declarator");
    }

    // TODO: array declerators go here
    while (accept(parser, TOK_OPEN_PAREN)) {
        AST *old_declarator = declarator;
        declarator = ast_new(AST_DECLARATOR_FUNC, declarator->main_token);
        ast_append(declarator, old_declarator);

        do {
            if (current_token(parser)->type == TOK_CLOSE_PAREN) {
                // TODO: handle this case properly
                // Really in this case we shouldn't provide a prototype. But
                // I really don't know why you'd use this syntax in the
                // modern age...

                // parse_error(parser, "empty param list doesn't provide a prototype");

                break;
            }

            if (accept(parser, TOK_KEYWORD_VOID)) {
                if (declarator->num_nodes != 1) {
                    parse_error(parser, "void after parameter!");
                }

                break;
            }

            // TODO: handle parameters without identifiers
            AST *param_declaration = parse_declaration(parser, true);
            param_declaration->type = AST_PARAMETER;
            ast_append(declarator, param_declaration);

            if (current_token(parser)->type != TOK_CLOSE_PAREN) {
                expect(parser, TOK_COMMA);
            }
        } while (current_token(parser)->type != TOK_CLOSE_PAREN);

        expect(parser, TOK_CLOSE_PAREN);
    }

    return declarator;
}

static AST *parse_declarator_and_initialiser(Parser *parser) {
    AST *declarator_group = ast_new(AST_DECLARATOR_GROUP, current_token(parser));
    AST *declarator = parse_declarator(parser);
    ast_append(declarator_group, declarator);

    if (accept(parser, TOK_EQUALS)) {
        AST *initialiser = parse_expression(parser);
        ast_append(declarator_group, initialiser);
    }

    return declarator_group;
}

static AST *parse_declaration(Parser *parser, bool as_parameter) {
    AST *declaration = ast_new(AST_DECLARATION, current_token(parser));
    AST *specifier_part = ast_new(AST_DECLARATION_SPECIFIERS, current_token(parser));
    ast_append(declaration, specifier_part);

    while (current_token_is_specifier(parser)) {
        AST *specifier = ast_new(AST_DECLARATION_SPECIFIER, accept(parser, current_token(parser)->type));
        specifier->identifier_string = specifier->main_token->contents;
        ast_append(specifier_part, specifier);
    }

    do {
        ast_append(declaration, parse_declarator_and_initialiser(parser));
    } while (!as_parameter && accept(parser, TOK_COMMA));

    if (accept(parser, TOK_OPEN_CURLY)) {
        if (declaration->num_nodes != 2) {
            parse_error_prev(parser, "function definition can only have one declarator");
        }

        declaration->type = AST_FUNCTION_DEFINITION;
        ast_append(declaration, parse_block(parser));
        return declaration;
    }

    if (!as_parameter) {
        expect(parser, TOK_SEMICOLON);
    }

    return declaration;
}

static AST *parse_unit(Parser *parser) {
    AST *program_ast = ast_new(AST_PROGRAM, current_token(parser));

    while (current_token(parser)->type != TOK_EOF) {
        AST *declaration_ast = parse_declaration(parser, false);
        ast_append(program_ast, declaration_ast);
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