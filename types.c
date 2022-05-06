#include "xcc.h"

static Type *type_list_head = NULL;
static Type *type_list_tail = NULL;

static Type *type_new(void) {
    Type *new_type = xcc_malloc(sizeof(Type));
    new_type->next_type = NULL;

    if(!type_list_head) {
        type_list_head = new_type;
    }

    if(type_list_tail) {
        type_list_tail->next_type = new_type;
    }
    type_list_tail = new_type;

    return new_type;
}

static Type *try_make_common_int_type(TypeInteger integer_type, bool is_const) {
    // TODO: this is ugly

    static bool has_initialised_types = false;
    static bool is_initialising_types = false;

    if (is_initialising_types) {
        return NULL;
    }

    static Type *nonconst_int;
    static Type *const_int;
    static Type *nonconst_char;
    static Type *const_char;


    if (!has_initialised_types) {
        is_initialising_types = true;

        nonconst_int = type_new_int(TYPE_INT, false);
        const_int = type_new_int(TYPE_INT, true);

        nonconst_char = type_new_int(TYPE_CHAR, false);
        const_char = type_new_int(TYPE_CHAR, true);

        has_initialised_types = true;
    }

    if (integer_type == TYPE_INT && !is_const) {
        return nonconst_int;
    } else if (integer_type == TYPE_INT && is_const) {
        return const_int;
    } else if (integer_type == TYPE_CHAR && !is_const) {
        return nonconst_char;
    } else if (integer_type == TYPE_CHAR && is_const) {
        return const_char;
    }

    return NULL;
}

Type *type_new_int(TypeInteger integer_type, bool is_const) {
    Type *possible_common_type = try_make_common_int_type(
        integer_type, is_const
    );
    if (possible_common_type) {
        return possible_common_type;
    }

    Type *new_type = type_new();

    new_type->type_type = TYPE_INTEGER;
    new_type->integer_type = integer_type;
    new_type->is_const = is_const;
    new_type->underlying = NULL;

    return new_type;
}

static Type *type_new_void() {
    static Type *void_type;

    if (void_type == NULL) {
        void_type = type_new();
        void_type->type_type = TYPE_VOID;
        void_type->is_const = false;
        void_type->underlying = NULL;
    }

    return void_type;
}

static Type *copy_type(Type *type) {
    Type *new_type = type_new();

    new_type->type_type = type->type_type;
    new_type->integer_type = type->integer_type;
    new_type->is_const = type->is_const;
    new_type->array_size = type->array_size;
    new_type->underlying = type->underlying;
    new_type->next_type = type->next_type;

    return new_type;
}

static Type *make_unqualified_type(Type *type) {
    if (type->is_const) {
        Type *new_type = copy_type(type);

        new_type->is_const = false;
        return new_type;
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

        return t->array_size == u->array_size;
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
    }

    xcc_assert_not_reached();
}

static bool is_scalar_type(Type *type) {
    TypeType type_type = type->type_type; // type type type type type type type

    // TODO: TYPE_FLOAT
    return type_type == TYPE_INTEGER || type_type == TYPE_POINTER;
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
            return type_new_int(TYPE_INT, type->is_const);
        } else {
            return type_new_int(TYPE_UINT, type->is_const);
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
    if (expr->value_type->is_const) {
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
    parent_expr->value_type = type_new_int(TYPE_INT, false);
}

#define TYPE_PROPOGATE_RECURSE(ast) for(int i = 0; i < (ast)->num_nodes; ++i) { type_propogate(ast->nodes[i]); }

void type_propogate(AST *ast) {
    xcc_assert(ast->value_type == NULL);

    if (ast->type == AST_PROGRAM) {
        TYPE_PROPOGATE_RECURSE(ast);
    } else if (ast->type == AST_FUNCTION_PROTOTYPE || ast->type == AST_FUNCTION) {
        xcc_assert(ast->num_nodes >= 2);

        // return type
        type_propogate(ast->nodes[0]);
        xcc_assert(ast->nodes[0]->value_type);
        // param list
        type_propogate(ast->nodes[1]);


        xcc_assert(ast->function_res);

        // I think this check is also done as well at the resolution stage...
        xcc_assert(ast->function_res->num_arguments == ast->nodes[1]->num_nodes);

        if (ast->function_res->return_type) {
            for (int i = 0; i < ast->function_res->num_arguments; ++i) {
                Type *this_arg_type = ast->nodes[1]->nodes[i]->value_type;
                xcc_assert(this_arg_type);

                Type *other_arg_type = ast->function_res->argument_types[i];

                if (!types_are_compatible(this_arg_type, other_arg_type)) {
                    prog_error_ast(
                        "Incompatible arg type!", ast->nodes[1]->nodes[i]
                    );
                }
            }

            Type *this_return_type = ast->nodes[0]->value_type;
            Type *other_return_type = ast->function_res->return_type;
            xcc_assert(other_return_type);

            if(!types_are_compatible(other_return_type, this_return_type)) {
                prog_error_ast("Incompatible return type type!", ast->nodes[0]);
            }
        } else {
            ast->function_res->argument_types = xcc_malloc(
                sizeof(Type *) * ast->function_res->num_arguments
            );

            for (int i = 0; i < ast->function_res->num_arguments; ++i) {
                Type *this_arg_type = ast->nodes[1]->nodes[i]->value_type;
                xcc_assert(this_arg_type);

                ast->function_res->argument_types[i] = this_arg_type;
            }

            ast->function_res->return_type = ast->nodes[0]->value_type;
        }

        if (ast->type == AST_FUNCTION) {
            xcc_assert(ast->num_nodes == 3);

            // insert implicit return at end of void functions
            if (ast->function_res->return_type->type_type == TYPE_VOID) {
                AST *new_return_stmt = ast_append_new(
                    ast->nodes[2], AST_RETURN_STMT, ast->main_token
                );
                new_return_stmt->function_res = ast->function_res;
            }

            type_propogate(ast->nodes[2]);
        } else {
            xcc_assert(ast->type == AST_FUNCTION_PROTOTYPE);
            xcc_assert(ast->num_nodes == 2);
        }
    } else if (ast->type == AST_FUNC_DECL_PARAM_LIST) {
        // not much to do here i think
        TYPE_PROPOGATE_RECURSE(ast);
    } else if (ast->type == AST_VAR_DECLARE) {
        xcc_assert(ast->num_nodes >= 1);
        xcc_assert(ast->num_nodes <= 2);

        type_propogate(ast->nodes[0]);
        xcc_assert(ast->nodes[0]->value_type);

        if (ast->num_nodes == 2) {
            type_propogate(ast->nodes[1]);
            implicitly_convert(&ast->nodes[1], ast->nodes[0]->value_type);
        }

        xcc_assert(ast->var_res);
        xcc_assert(!ast->var_res->var_type);

        ast->var_res->var_type = ast->nodes[0]->value_type;
    } else if (ast->type == AST_VAR_USE) {
        xcc_assert(ast->num_nodes == 0);

        xcc_assert(ast->var_res);
        xcc_assert(ast->var_res->var_type);

        ast->value_type = ast->var_res->var_type;
    } else if (ast->type == AST_CALL) {
        // not much to do here i think
        TYPE_PROPOGATE_RECURSE(ast);

        xcc_assert(ast->function_res);
        xcc_assert(ast->num_nodes == ast->function_res->num_arguments);
        xcc_assert(ast->function_res->return_type);

        for (int i = 0; i < ast->num_nodes; ++i) {
            implicitly_convert(&ast->nodes[i], ast->function_res->argument_types[i]);
        }

        ast->value_type = ast->function_res->return_type;
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
    } else if (ast->type == AST_PARAMETER) {
        xcc_assert(ast->num_nodes == 1);
        type_propogate(ast->nodes[0]);

        xcc_assert(ast->nodes[0]->value_type);
        ast->value_type = ast->nodes[0]->value_type;

        // AST_PARAMETERs on prototypes don't have a resolution
        if (ast->var_res) {
            xcc_assert(!ast->var_res->var_type);

            ast->var_res->var_type = ast->nodes[0]->value_type;
        }
    } else if (ast->type == AST_TYPE) {
        // TODO: do this better
        xcc_assert(ast->num_nodes == 1);

        if (ast->nodes[0]->type == AST_TYPE_INT) {
            ast->value_type = type_new_int(TYPE_INT, false);
        } else if (ast->nodes[0]->type == AST_TYPE_CHAR) {
            ast->value_type = type_new_int(TYPE_CHAR, false);
        } else if (ast->nodes[0]->type == AST_TYPE_VOID) {
            ast->value_type = type_new_void();
        } else {
            xcc_assert_not_reached_msg("invalid contents of AST_TYPE");
        }
    } else if (ast->type == AST_INTEGER_LITERAL) {
        ast->value_type = type_new_int(TYPE_INT, false);
    } else if (ast->type == AST_RETURN_STMT) {
        if (ast->num_nodes == 1) {
            type_propogate(ast->nodes[0]);

            xcc_assert(ast->function_res);
            Type *return_type = ast->function_res->return_type;
            xcc_assert(return_type);

            implicitly_convert(&ast->nodes[0], return_type);
        } else {
            xcc_assert(ast->num_nodes == 0);

            if (ast->function_res->return_type->type_type != TYPE_VOID) {
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
    } else if (ast->type == AST_BODY) {
        TYPE_PROPOGATE_RECURSE(ast);
    } else if (ast->type == AST_BLOCK_STATEMENT) {
        TYPE_PROPOGATE_RECURSE(ast);
    } else if (ast->type == AST_IF) {
        xcc_assert(ast->num_nodes == 2 || ast->num_nodes == 3);
        TYPE_PROPOGATE_RECURSE(ast);

        if (!is_scalar_type(ast->nodes[0]->value_type)) {
            prog_error_ast("if condition needs scalar type!", ast);
        }
    } else {
        xcc_assert_not_reached_msg("AST type not handled in type propogator!");
    }
}

void type_free_all() {
    Type *type = type_list_head;

    while(type) {
        Type *next_type = type->next_type;
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
    }

    xcc_assert_not_reached();
}

void type_dump(Type *type) {
    fprintf(stderr, "[TYPE ");

    if (type->is_const) {
        fprintf(stderr, "const ");
    }

    if(type->type_type == TYPE_INTEGER) {
        fprintf(stderr, "%s", int_type_to_string(type));
    } else if (type->type_type == TYPE_VOID) {
        fprintf(stderr, "void");
    } else {
        xcc_assert_not_reached();
    }

    fprintf(stderr, "]");
}