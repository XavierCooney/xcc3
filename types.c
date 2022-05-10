#include "xcc.h"

static Type *type_list_head = NULL;
static Type *type_list_tail = NULL;

static void initialise_type(Type *new_type) {
    new_type->next_type = NULL;
    new_type->function_param_types = NULL;
    new_type->underlying = NULL;
    new_type->is_const = false;
    new_type->is_volatile = false;
    new_type->is_restrict = false;
}

static Type *type_new(void) {
    Type *new_type = xcc_malloc(sizeof(Type));
    initialise_type(new_type);

    if(!type_list_head) {
        type_list_head = new_type;
    }

    if(type_list_tail) {
        type_list_tail->next_type = new_type;
    }
    type_list_tail = new_type;

    return new_type;
}

static Type prebuilt_integer_types[TYPE_INTEGER_LAST * 4];
static bool prebuilt_integer_types_created = false;

static void create_prebuilt_integer_types(void) {
    for (int int_type = 0; int_type < TYPE_INTEGER_LAST; ++int_type) {
        for (int qualifiers = 0; qualifiers < 4; ++qualifiers) {
            Type *t = &prebuilt_integer_types[int_type << 2 | qualifiers];
            initialise_type(t);
            t->integer_type = int_type;
            t->type_type = TYPE_INTEGER;
            t->is_const = qualifiers & 1;
            t->is_volatile = qualifiers & 2;
            t->is_restrict = false;
        }
    }
}

static Type *try_make_common_int_type(TypeInteger integer_type, bool is_const, bool is_volatile) {
    if (!prebuilt_integer_types_created) {
        create_prebuilt_integer_types();
        prebuilt_integer_types_created = true;
    }

    int index = is_const << 0 | is_volatile << 1 | integer_type << 2;

    return &prebuilt_integer_types[index];
}

Type *type_new_int(TypeInteger integer_type, bool is_const, bool is_volatile) {
    Type *possible_common_type = try_make_common_int_type(
        integer_type, is_const, is_volatile
    );
    if (possible_common_type) {
        return possible_common_type;
    }

    Type *new_type = type_new();

    new_type->type_type = TYPE_INTEGER;
    new_type->integer_type = integer_type;
    new_type->is_const = is_const;
    new_type->is_volatile = is_volatile;
    new_type->is_restrict = false;

    return new_type;
}

static Type *type_new_void(void) {
    static Type *void_type = NULL;

    if (void_type == NULL) {
        void_type = type_new();
        void_type->type_type = TYPE_VOID;
    }

    return void_type;
}

static Type *copy_type(Type *type) {
    Type *new_type = type_new();

    new_type->type_type = type->type_type;
    new_type->integer_type = type->integer_type;
    new_type->is_const = type->is_const;
    new_type->is_volatile = type->is_volatile;
    new_type->is_restrict = type->is_restrict;
    new_type->array_size = type->array_size;
    new_type->underlying = type->underlying;
    new_type->function_param_types = type->function_param_types;
    new_type->next_type = type->next_type;

    return new_type;
}

static bool is_type_qualified(Type *type) {
    return type->is_const || type->is_volatile || type->is_restrict;
}

static Type *make_unqualified_type(Type *type) {
    if (is_type_qualified(type)) {
        if (type->type_type == TYPE_VOID) {
            return type_new_void();
        } else if (type->type_type == TYPE_INTEGER) {
            return type_new_int(type->integer_type, 0, 0);
        } else {
            Type *new_type = copy_type(type);

            new_type->is_const = false;
            new_type->is_volatile = false;
            new_type->is_restrict = false;
            return new_type;
        }
    } else {
        return type;
    }
}

static bool types_are_compatible(Type *t, Type *u) {
    // compatible is the stricter version
    // https://en.cppreference.com/w/c/language/type#Compatible_types

    xcc_assert(t);
    xcc_assert(u);

    if(t->is_const != u->is_const) {
        return false;
    }
    if(t->is_volatile != u->is_volatile) {
        return false;
    }
    if(t->is_restrict != u->is_restrict) {
        return false;
    }

    bool has_same_type_type = t->type_type == u->type_type;

    if (has_same_type_type && t->type_type == TYPE_INTEGER) {
        return t->integer_type == u->integer_type;
    }

    if (has_same_type_type && t->type_type == TYPE_POINTER) {
        return types_are_compatible(t->underlying, u->underlying);
    }

    if (has_same_type_type && t->type_type == TYPE_ARRAY) {
        if (t->array_size == -1 || u->array_size == -1) {
            return true;
        }

        return t->array_size == u->array_size && types_are_compatible(t->underlying, u->underlying);
    }

    if (has_same_type_type && t->type_type == TYPE_FUNCTION) {
        if (t->array_size != u->array_size) {
            return false;
        }

        if (!types_are_compatible(t->underlying, u->underlying)) {
            return false;
        }

        for (int i = 0; i < t->array_size; ++i) {
            if (!types_are_compatible(t->function_param_types[i], u->function_param_types[i])) {
                return false;
            }
        }

        return true;
    }


    if (has_same_type_type && t->type_type == TYPE_STRUCT) {
        xcc_assert_not_reached();
    }

    if (has_same_type_type && t->type_type == TYPE_UNION) {
        xcc_assert_not_reached();
    }

    if (t->type_type == TYPE_ENUM || u->type_type == TYPE_ENUM) {
        xcc_assert_not_reached();
    }

    return false;
}

static int get_integer_rank(TypeInteger integer_type) {
    switch (integer_type) {
        case TYPE_BOOL: return 10;

        case TYPE_CHAR:
        case TYPE_SCHAR:
        case TYPE_UCHAR: return 12;

        case TYPE_SHORT:
        case TYPE_USHORT: return 14;

        case TYPE_INT:
        case TYPE_UINT: return 16;

        case TYPE_LONG:
        case TYPE_ULONG: return 18;

        case TYPE_LONG_LONG:
        case TYPE_ULONG_LONG: return 20;

        case TYPE_INTEGER_LAST: xcc_assert_not_reached();
    }

    xcc_assert_not_reached();
}

static int get_integer_rank_from_type(Type *type) {
    xcc_assert(type->type_type == TYPE_INTEGER);

    return get_integer_rank(type->integer_type);
}

bool integer_type_is_signed(Type *type) {
    xcc_assert(type->type_type == TYPE_INTEGER);

    switch (type->integer_type) {
        case TYPE_BOOL: return false;

        case TYPE_CHAR: return true; // System V ABI
        case TYPE_SCHAR: return true;
        case TYPE_UCHAR: return false;

        case TYPE_SHORT: return true;
        case TYPE_USHORT: return false;

        case TYPE_INT: return true;
        case TYPE_UINT: return false;

        case TYPE_LONG: return true;
        case TYPE_ULONG: return false;

        case TYPE_LONG_LONG: return true;
        case TYPE_ULONG_LONG: return false;

        case TYPE_INTEGER_LAST: xcc_assert_not_reached();
    }

    xcc_assert_not_reached();
}

static bool is_scalar_type(Type *type) {
    TypeType type_type = type->type_type; // type type type type type type type

    // TODO: TYPE_FLOAT
    return type_type == TYPE_INTEGER || type_type == TYPE_POINTER || type_type == TYPE_ENUM;
}

static AST* add_conversion_in_ast(AST **ast_ptr, ASTType ast_type) {
    // TODO: the token should probably be something else...
    AST *old_ast = *ast_ptr;

    AST *new_ast = ast_new(ast_type, old_ast->main_token);
    *ast_ptr = new_ast;

    ast_append(new_ast, old_ast);

    return new_ast;
}

static Type *promote_integer(Type *type) {
    if (get_integer_rank_from_type(type) <= get_integer_rank(TYPE_INT)) {
        if (integer_type_is_signed(type)) {
            return type_new_int(TYPE_INT, type->is_const, type->is_volatile);
        } else {
            return type_new_int(TYPE_UINT, type->is_const, type->is_volatile);
        }
    }

    return type;
}

static void implicitly_convert(AST **expr_ptr, Type *desired) {
    // https://en.cppreference.com/w/c/language/conversion#Implicit_conversion_semantics

    AST *old_ast = *expr_ptr;

    if (types_are_compatible(old_ast->value_type, desired)) {
        return;
    }

    if (desired->type_type == TYPE_INTEGER && desired->integer_type == TYPE_BOOL) {
        if (!is_scalar_type(old_ast->value_type)) {
            prog_error_ast("Cannot implicitly convert non-scalar to bool", old_ast);
        }

        AST *new_ast = add_conversion_in_ast(expr_ptr, AST_CONVERT_TO_BOOL);
        new_ast->value_type = desired;

        return;
    } else if (desired->type_type == TYPE_INTEGER) {
        // TODO: float
        if (old_ast->value_type->type_type == TYPE_INTEGER) {
            AST *new_ast = add_conversion_in_ast(expr_ptr, AST_CONVERT_TO_INT);
            new_ast->value_type = desired;

            return;
        }

    }

    // TODO: so much more...

    // TODO: better error message!
    begin_prog_error_range("Cannot implicitly convert type", old_ast->main_token, old_ast->main_token);
    fprintf(stderr, "The expression has type ");
    type_dump(old_ast->value_type);
    fprintf(stderr, ", but the expected type was ");
    type_dump(desired);
    fprintf(stderr, "!\n");
    end_prog_error();
}

static void use_as_rvalue(AST *expr) {
    if (is_type_qualified(expr->value_type)) {
        expr->value_type = make_unqualified_type(expr->value_type);
    }
}

static void perform_binary_arithmetic_conversion(AST *parent_expr) {
    xcc_assert(parent_expr->num_nodes == 2); // binary

    use_as_rvalue(parent_expr->nodes[0]);
    use_as_rvalue(parent_expr->nodes[1]);

    AST **left_operand_ptr = &parent_expr->nodes[0];
    AST **right_operand_ptr = &parent_expr->nodes[1];

    AST *ast_left_old = parent_expr->nodes[0];
    AST *ast_right_old = parent_expr->nodes[1];

    Type *type_left = ast_left_old->value_type;
    Type *type_right = ast_right_old->value_type;

    // TODO: handle floats, before integer promotion
    if (type_left->type_type != TYPE_INTEGER) {
        prog_error_ast("Expected an integer type", ast_left_old);
    }

    if (type_right->type_type != TYPE_INTEGER) {
        prog_error_ast("Expected an integer type", ast_right_old);
    }

    Type *result_type = NULL;

    type_left = promote_integer(type_left);
    type_right = promote_integer(type_right);

    bool left_is_signed = integer_type_is_signed(type_left);
    bool right_is_signed = integer_type_is_signed(type_left);

    int left_rank = get_integer_rank_from_type(type_left);
    int right_rank = get_integer_rank_from_type(type_right);

    if (type_left->integer_type == type_right->integer_type) {
        result_type = type_left;
    } else if (left_is_signed == right_is_signed) {
        if (left_rank < right_rank) {
            result_type = type_right;
        } else {
            xcc_assert(left_rank > left_rank);
            result_type = type_left;
        }
    } else {
        // just a couple more cases that I couldn't be bothered to do...
        xcc_assert_not_reached_msg("TODO: type conversion of mixed signdness");
    }

    implicitly_convert(left_operand_ptr, result_type);
    implicitly_convert(right_operand_ptr, result_type);

    xcc_assert(result_type);
    xcc_assert(types_are_compatible(
        result_type,
        parent_expr->nodes[0]->value_type
    ));
    xcc_assert(types_are_compatible(
        result_type,
        parent_expr->nodes[1]->value_type
    ));
    xcc_assert(types_are_compatible(
        parent_expr->nodes[0]->value_type,
        parent_expr->nodes[1]->value_type
    ));

    parent_expr->value_type = result_type;
}

static void handle_comparison_operator(AST *parent_expr) {
    perform_binary_arithmetic_conversion(parent_expr);
    xcc_assert(parent_expr->value_type);
    // comparison results are always an int
    parent_expr->value_type = type_new_int(TYPE_INT, 0, 0);
}

#define TYPE_PROPOGATE_RECURSE(ast) for(int i = 0; i < (ast)->num_nodes; ++i) { type_propogate(ast->nodes[i]); }

static void check_main_function(AST *ast) {
    Type *function_type = ast->declaration->type;
    xcc_assert(function_type);

    if (!types_are_compatible(function_type->underlying, type_new_int(TYPE_INT, false, false))) {
        prog_error_ast("main should return an int!", ast);
    }

    if (function_type->array_size != 0) {
        prog_error_ast("main shouldn't accept any arguments!", ast);
    }

    AST *return_statement = ast_append_new(ast->nodes[2], AST_RETURN_STMT, ast->main_token);
    return_statement->declaration = ast->declaration;
    AST *literal_0 = ast_append_new(return_statement, AST_INTEGER_LITERAL, ast->main_token);
    literal_0->integer_literal_val = 0;
}

static void handle_declaration(AST *ast) {
    xcc_assert(ast->type == AST_DECLARATION || ast->type == AST_FUNCTION_DEFINITION || ast->type == AST_PARAMETER);
    xcc_assert(ast->num_nodes >= 1);

    type_propogate(ast->nodes[0]);
    Type *base_type = ast->nodes[0]->value_type;

    if (ast->type == AST_FUNCTION_DEFINITION) {
        xcc_assert(ast->num_nodes == 3);

        ast->nodes[1]->value_type = base_type;
        type_propogate(ast->nodes[1]); // DECLARATOR_GROUP


        // implicit return for void return and main()
        // doesn't really make sense to put it here but why not
        Type *function_type = ast->declaration->type;
        xcc_assert(function_type);

        if (function_type->type_type != TYPE_FUNCTION) {
            prog_error_ast("invalid function definition", ast);
        }

        if (function_type->underlying->type_type == TYPE_FUNCTION) {
            prog_error_ast("can't return a bare function", ast);
        }
        for (int i = 0; i < function_type->array_size; ++i) {
            if (function_type->function_param_types[i]->type_type == TYPE_FUNCTION) {
                prog_error_ast("can't have a bare function as a parameter", ast);
            }
        }

        if (function_type->underlying->type_type == TYPE_VOID) {
            AST *return_statement = ast_append_new(ast->nodes[2], AST_RETURN_STMT, ast->main_token);
            return_statement->declaration = ast->declaration;
        }
        if (!strcmp(ast->declaration->name, "main")) {
            check_main_function(ast);
        }

        type_propogate(ast->nodes[2]); // BLOCK_STATEMENT
    } else if (ast->type == AST_PARAMETER) {
        xcc_assert(ast->num_nodes == 2);

        ast->nodes[1]->value_type = base_type;
        type_propogate(ast->nodes[1]); // DECLARATOR_GROUP
        ast->value_type = ast->nodes[1]->declaration->type;
    } else {
        for (int i = 1; i < ast->num_nodes; ++i) {
            ast->nodes[i]->value_type = base_type;
            type_propogate(ast->nodes[i]);
        }
    }
}

static int count_specifiers(AST *ast, TokenType token_type, bool allow_duplicates) {
    int count = 0;

    AST *last_matching_node = NULL;

    for (int i = 0; i < ast->num_nodes; ++i) {
        xcc_assert(ast->nodes[i]->type == AST_DECLARATION_SPECIFIER);
        if (ast->nodes[i]->main_token->type == token_type) {
            last_matching_node = ast->nodes[i];
            count++;
        }
    }

    if (count > 1 && !allow_duplicates) {
        prog_error_ast("duplicate specifier", last_matching_node);
    }

    return count;
}

static void handle_declaration_specifiers(AST *ast) {
    // TODO: handle the case where there isn't an int...

    xcc_assert(ast->type == AST_DECLARATION_SPECIFIERS);
    if (ast->num_nodes < 1) {
        prog_error_ast("not type specified (and I won't assume int...)", ast);
    }


    int void_count = count_specifiers(ast, TOK_KEYWORD_VOID, false);

    int signed_count = count_specifiers(ast, TOK_KEYWORD_SIGNED, false);
    int unsigned_count = count_specifiers(ast, TOK_KEYWORD_UNSIGNED, false);
    if (unsigned_count && signed_count) {
        prog_error_ast("both signed and unsigned in type specifier", ast);
    }

    int char_count = count_specifiers(ast, TOK_KEYWORD_CHAR, false);
    int short_count = count_specifiers(ast, TOK_KEYWORD_SHORT, false);
    int int_count = count_specifiers(ast, TOK_KEYWORD_INT, false);
    int long_count = count_specifiers(ast, TOK_KEYWORD_LONG, true);
    if (long_count > 2) {
        prog_error_ast("more than two 'long's in type specifier", ast);
    }

    int total_types_count = char_count + short_count + int_count + !!long_count + void_count;
    if (total_types_count > 1) {
        prog_error_ast("too many types in specifier", ast);
    }

    if (void_count) {
        if (signed_count || unsigned_count) {
            prog_error_ast("void doesn't have signedness!", ast);
        }
        ast->value_type = type_new_void();
        return;
    }

    TypeInteger integer_type;

    if (char_count) {
        if (signed_count) {
            integer_type = TYPE_SCHAR;
        } else if (unsigned_count) {
            integer_type = TYPE_UCHAR;
        } else {
            integer_type = TYPE_CHAR;
        }
    } else if (short_count) {
        if (unsigned_count) {
            integer_type = TYPE_USHORT;
        } else {
            integer_type = TYPE_SHORT;
        }
    } else if (long_count == 2) {
        if (unsigned_count) {
            integer_type = TYPE_ULONG_LONG;
        } else {
            integer_type = TYPE_LONG_LONG;
        }
    } else if (long_count == 1) {
        if (unsigned_count) {
            integer_type = TYPE_ULONG;
        } else {
            integer_type = TYPE_LONG;
        }
    } else if (unsigned_count) {
        integer_type = TYPE_UINT;
    } else if (signed_count || int_count) {
        integer_type = TYPE_INT;
    } else {
        prog_error_ast("unknown type specification", ast);
    }

    // TODO: handle qualifiers
    ast->value_type = type_new_int(integer_type, false, false);
}

static void handle_declarator_group(AST *ast) {
    xcc_assert(ast->type == AST_DECLARATOR_GROUP);

    xcc_assert(ast->num_nodes >= 1 && ast->num_nodes <= 2);
    xcc_assert(ast->value_type); // type given to us by the AST_DECLARATION from the base type

    xcc_assert(ast->declaration);
    // xcc_assert(!ast->declaration->type);

    ast->nodes[0]->value_type = ast->value_type;
    type_propogate(ast->nodes[0]);

    xcc_assert(ast->declaration->type);

    if (ast->num_nodes == 2) {
        // has initialiser
        type_propogate(ast->nodes[1]);
        implicitly_convert(&ast->nodes[1], ast->declaration->type);
    }
}

static void handle_declarator_ident(AST *ast) {
    xcc_assert(ast->type == AST_DECLARATOR_IDENT);
    xcc_assert(ast->value_type);
    xcc_assert(ast->declaration);

    if (ast->declaration->type) {
        if (!types_are_compatible(ast->value_type, ast->declaration->type)) {
            prog_error_ast("redeclaration with incompatible types", ast);
        }
    }
    ast->declaration->type = ast->value_type;
}

static void handle_declarator_func(AST *ast) {
    xcc_assert(ast->type == AST_DECLARATOR_FUNC);
    xcc_assert(ast->value_type);
    xcc_assert(ast->num_nodes >= 1);

    Type *return_type = ast->value_type;
    int num_params = ast->num_nodes - 1;

    Type **paramater_types = xcc_malloc(sizeof(Type *) * num_params);
    for (int i = 0; i < num_params; ++i) {
        type_propogate(ast->nodes[i + 1]);
        paramater_types[i] = ast->nodes[i + 1]->value_type;
    }

    Type *function_type = type_new();
    function_type->type_type = TYPE_FUNCTION;
    function_type->underlying = return_type;
    function_type->function_param_types = paramater_types;
    function_type->array_size = num_params;

    ast->nodes[0]->value_type = function_type;
    type_propogate(ast->nodes[0]);
}

static void handle_call(AST *ast) {
    xcc_assert(ast->num_nodes >= 1);

    TYPE_PROPOGATE_RECURSE(ast);

    // TODO: handle function pointers

    if (ast->nodes[0]->type != AST_IDENT_USE) {
        prog_error_ast("can't use this to call", ast->nodes[0]);
    }

    Type *function_type = ast->nodes[0]->value_type;

    if (function_type->type_type != TYPE_FUNCTION) {
        prog_error_ast("can only call functions", ast->nodes[0]);
    }

    int num_params = ast->num_nodes - 1;
    if (function_type->array_size != num_params) {
        prog_error_ast("mimsatch in number of parameters in call", ast);
    }

    for (int i = 0; i < ast->num_nodes - 1; ++i) {
        implicitly_convert(&ast->nodes[i + 1], function_type->function_param_types[i]);
    }

    ast->value_type = ast->nodes[0]->value_type->underlying;
}

void type_propogate(AST *ast) {
    if (ast->type == AST_PROGRAM) {
        TYPE_PROPOGATE_RECURSE(ast);
    } else if (ast->type == AST_DECLARATION || ast->type == AST_FUNCTION_DEFINITION || ast->type == AST_PARAMETER) {
        handle_declaration(ast);
    } else if (ast->type == AST_DECLARATION_SPECIFIERS) {
        handle_declaration_specifiers(ast);
    } else if (ast->type == AST_DECLARATOR_IDENT) {
        handle_declarator_ident(ast);
    } else if (ast->type == AST_DECLARATOR_GROUP) {
        handle_declarator_group(ast);
    } else if (ast->type == AST_DECLARATOR_FUNC) {
        handle_declarator_func(ast);
    } else if (ast->type == AST_CALL) {
        handle_call(ast);
    } else if (ast->type == AST_IDENT_USE) {
        xcc_assert(ast->declaration);
        xcc_assert(ast->declaration->type);
        ast->value_type = ast->declaration->type;
    } else if (ast->type == AST_ASSIGN) {
        // lvalue checking is done in check_lvalue.c
        xcc_assert(ast->num_nodes == 2);
        type_propogate(ast->nodes[0]);
        type_propogate(ast->nodes[1]);

        Type *var_type = ast->nodes[0]->value_type;
        xcc_assert(var_type);

        if (var_type->is_const) {
            prog_error_ast("Can't assign to a const var!", ast);
        }

        implicitly_convert(&ast->nodes[1], var_type);

        ast->value_type = var_type;
    } else if (ast->type == AST_INTEGER_LITERAL) {
        ast->value_type = type_new_int(TYPE_INT, false, false);
    } else if (ast->type == AST_RETURN_STMT) {
        xcc_assert(ast->declaration);
        xcc_assert(ast->declaration->type);
        Type *return_type = ast->declaration->type->underlying;

        if (ast->num_nodes == 1) {
            type_propogate(ast->nodes[0]);


            implicitly_convert(&ast->nodes[0], return_type);
        } else {
            xcc_assert(ast->num_nodes == 0);

            if (return_type->type_type != TYPE_VOID) {
                prog_error_ast("empty return in non-void function", ast);
            }
        }
    } else if (ast->type == AST_ADD) {
        TYPE_PROPOGATE_RECURSE(ast);
        perform_binary_arithmetic_conversion(ast);
        xcc_assert(ast->value_type != NULL);
    } else if (ast->type == AST_SUBTRACT) {
        TYPE_PROPOGATE_RECURSE(ast);
        perform_binary_arithmetic_conversion(ast);
        xcc_assert(ast->value_type != NULL);
    } else if (ast->type == AST_MULTIPLY) {
        TYPE_PROPOGATE_RECURSE(ast);
        perform_binary_arithmetic_conversion(ast);
        xcc_assert(ast->value_type != NULL);
    } else if (ast->type == AST_CMP_LT) {
        TYPE_PROPOGATE_RECURSE(ast);
        handle_comparison_operator(ast);
    } else if (ast->type == AST_CMP_LT_EQ) {
        TYPE_PROPOGATE_RECURSE(ast);
        handle_comparison_operator(ast);
    } else if (ast->type == AST_CMP_GT) {
        TYPE_PROPOGATE_RECURSE(ast);
        handle_comparison_operator(ast);
    } else if (ast->type == AST_CMP_GT_EQ) {
        TYPE_PROPOGATE_RECURSE(ast);
        handle_comparison_operator(ast);
    } else if (ast->type == AST_STATEMENT_EXPRESSION) {
        TYPE_PROPOGATE_RECURSE(ast); // nothing to do here
    } else if (ast->type == AST_BLOCK_STATEMENT) {
        TYPE_PROPOGATE_RECURSE(ast);
    } else if (ast->type == AST_IF) {
        xcc_assert(ast->num_nodes == 2 || ast->num_nodes == 3);
        TYPE_PROPOGATE_RECURSE(ast);

        if (!is_scalar_type(ast->nodes[0]->value_type)) {
            prog_error_ast("if condition needs scalar type!", ast);
        }
    } else if (ast->type == AST_WHILE) {
        xcc_assert(ast->num_nodes == 2);
        TYPE_PROPOGATE_RECURSE(ast);

        if (!is_scalar_type(ast->nodes[0]->value_type)) {
            prog_error_ast("while condition needs scalar type!", ast);
        }
    } else {
        xcc_assert_not_reached_msg("AST type not handled in type propogator!");
    }
}

void type_free_all() {
    Type *type = type_list_head;

    while(type) {
        Type *next_type = type->next_type;

        if (type->function_param_types) {
            xcc_free(type->function_param_types);
        }
        xcc_free(type);

        type = next_type;
    }
}

static const char *int_type_to_string(Type *type) {
    xcc_assert(type->type_type == TYPE_INTEGER);

    switch (type->integer_type) {
        case TYPE_BOOL: return "_Bool";
        case TYPE_CHAR: return "char";
        case TYPE_SCHAR: return "signed char";
        case TYPE_UCHAR: return "unsigned char";
        case TYPE_SHORT: return "short";
        case TYPE_USHORT: return "unsigned short";
        case TYPE_INT: return "int";
        case TYPE_UINT: return "unsigned int";
        case TYPE_LONG: return "long";
        case TYPE_ULONG: return "unsigned long";
        case TYPE_LONG_LONG: return "long long";
        case TYPE_ULONG_LONG: return "unsigned long long";
        case TYPE_INTEGER_LAST: xcc_assert_not_reached();
    }

    xcc_assert_not_reached();
}

void type_dump(Type *type) {
    // fprintf(stderr, "[");

    if (type->is_const) {
        fprintf(stderr, "const ");
    }
    if (type->is_volatile) {
        fprintf(stderr, "volatile ");
    }
    if (type->is_restrict) {
        fprintf(stderr, "restrict ");
    }

    if(type->type_type == TYPE_INTEGER) {
        fprintf(stderr, "%s", int_type_to_string(type));
    } else if (type->type_type == TYPE_VOID) {
        fprintf(stderr, "void");
    } else if (type->type_type == TYPE_FUNCTION) {
        fprintf(stderr, "(");
        for (int i = 0; i < type->array_size; ++i) {
            if (i != 0) {
                fprintf(stderr, ", ");
            }
            type_dump(type->function_param_types[i]);
        }
        fprintf(stderr, ") -> ");
        type_dump(type->underlying);
    } else {
        xcc_assert_not_reached();
    }

    // fprintf(stderr, "]");
}