// Name resolution
#include "xcc.h"

static void append_local_declaration_pointer(ResolutionList *res_list, Declaration *declaration) {
    Declaration **new_declaration_slot;
    LIST_STRUCT_APPEND_FUNC(
        Declaration *, res_list, num_local_declarations, num_local_declarations_allocated,
        local_declarations, new_declaration_slot
    );
    *new_declaration_slot = declaration;
}

static Declaration *append_empty_declaration(ResolutionList *res_list) {
    Declaration *declaration = xcc_malloc(sizeof(Declaration));

    declaration->name = NULL;
    declaration->type = NULL;
    // declaration->decl_type = decl_type;
    declaration->last_declaration_ast = NULL;
    declaration->definition_ast = NULL;
    declaration->next_in_list = res_list->all_declarations_head;

    res_list->all_declarations_head = declaration;

    append_local_declaration_pointer(res_list, declaration);

    return declaration;
}

static Declaration *check_for_duplicating_declaration(ResolutionList *res_list, const char *name, int scope_level, bool is_prototype, AST *ast) {
    // this can be done more efficiently by iterating from the back of the array and
    // then exiting early once the next scope level is reached
    for (int i = 0; i < res_list->num_local_declarations; ++i) {
        Declaration *declaration = res_list->local_declarations[i];
        if (declaration->scope_level == scope_level && !strcmp(declaration->name, name)) {
            // TODO: allow redeclaration of anything with linkage
            if (declaration->decl_type == DECL_FUNC_PROTOTYPE && is_prototype) return declaration;

            prog_error_ast("shadowing of existing declaration!", ast);
        }
    }
    return false;
}

static void handle_ident_declaration(ResolutionList *res_list, AST *ast, AST *parent, AST *declaration_root, AST *declarator_group, int scope_level) {
    xcc_assert(declaration_root);
    xcc_assert(ast->type == AST_DECLARATOR_IDENT);

    bool provides_func_prototype = parent->type == AST_DECLARATOR_FUNC;

    Declaration *duplicated_declaration = check_for_duplicating_declaration(
        res_list, ast->identifier_string, scope_level, provides_func_prototype, ast
    );

    Declaration *declaration;

    if (!duplicated_declaration) {
        declaration = append_empty_declaration(res_list);
        xcc_assert(ast->identifier_string);
        declaration->name = ast->identifier_string;
        declaration->scope_level = scope_level;
    } else {
        declaration = duplicated_declaration;
    }

    if (provides_func_prototype) {
        if (declaration_root->type == AST_FUNCTION_DEFINITION) {
            declaration->decl_type = DECL_FUNC_PROTOTYPE;
            declaration_root->declaration = declaration;
        } else {
            declaration->decl_type = DECL_FUNC_PROTOTYPE;
        }
    } else {
        if (declaration_root->type == AST_FUNCTION_DEFINITION) {
            prog_error_ast("function definition but not a function", ast);
        } else if (declaration_root->type == AST_PARAMETER) {
            declaration->decl_type = DECL_PARAM_TYPE;
            declaration_root->declaration = declaration;
        } else if (res_list->current_func) {
            declaration->decl_type = DECL_LOCAL_VAR;
        } else {
            declaration->decl_type = DECL_GLOBAL_VAR;
        }
    }

    ast->declaration = declaration;
    declaration->last_declaration_ast = ast;
    xcc_assert(declarator_group);
    declarator_group->declaration = declaration;

    if (declaration_root->type == AST_FUNCTION_DEFINITION) {
        if (declaration->definition_ast) {
            prog_error_ast("redefinition of function", ast);
        }

        declaration->definition_ast = ast;
    }
}

static void handle_ident_usage(ResolutionList *res_list, AST *ast) {
    const char *ident_name = ast->identifier_string;
    xcc_assert(ident_name);

    for (int i = res_list->num_local_declarations - 1; i >= 0; i--) {
        Declaration *d = res_list->local_declarations[i];
        if (d->name && !strcmp(d->name, ident_name)) {
            ast->declaration = d;
            return;
        }
    }

    prog_error_ast("unknown identifier", ast);
}

static void resolve_recursive(ResolutionList *res_list, AST *ast, AST *parent, AST *declaration_root, AST *declarator_group, int scope_level) {
    if (ast->type == AST_DECLARATOR_IDENT) {
        handle_ident_declaration(res_list, ast, parent, declaration_root, declarator_group, scope_level);
    } else if (ast->type == AST_IDENT_USE) {
        handle_ident_usage(res_list, ast);
    } else if (ast->type == AST_RETURN_STMT) {
        xcc_assert(res_list->current_func);
        xcc_assert(res_list->current_func->declaration);
        ast->declaration = res_list->current_func->declaration;
    }

    AST *old_declaration_root = declaration_root;

    if (ast->type == AST_DECLARATION || ast->type == AST_PARAMETER || ast->type == AST_FUNCTION_DEFINITION) {
        declaration_root = ast;
    }

    if (ast->type == AST_DECLARATOR_GROUP) {
        declarator_group = ast;

        if (declaration_root->type == AST_PARAMETER && ast->num_nodes == 2) {
            prog_error_ast("can't have an initialiser for parameter", ast);
        }
    }

    int old_num_locals = res_list->num_local_declarations;

    // declarations go out of scope
    //  - at the end of a BLOCK_STATEMENT
    //  - params at the end of a DECLARATOR_FUNC, except for params in
    //    function definitions, which last till the end of FUNCTION_DEFINITION
    bool is_scope_introduction = ast->type == AST_BLOCK_STATEMENT;

    int node_offset_index = 0;

    if (ast->type == AST_FUNCTION_DEFINITION) {
        if (res_list->current_func) {
            prog_error_ast("can't have functions in functions :(", ast);
        }

        res_list->current_func = ast;
        is_scope_introduction = true;

        // xcc_assert(ast->num_nodes == 3);
        // resolve_recursive(res_list, ast->nodes[0], ast, declaration_root, declarator_group, scope_level);

        // old_num_locals = res_list->num_local_declarations;
        // node_offset_index = 1;

        // resolve_recursive(res_list, ast->nodes[1], ast, declaration_root, declarator_group, scope_level);
    }

    if (ast->type == AST_DECLARATOR_FUNC && declaration_root->type != AST_FUNCTION_DEFINITION) {
        xcc_assert(ast->num_nodes >= 1);
        resolve_recursive(res_list, ast->nodes[0], ast, declaration_root, declarator_group, scope_level);

        old_num_locals = res_list->num_local_declarations;
        is_scope_introduction = true;

        node_offset_index = 1;
    }

    for (int i = node_offset_index; i < ast->num_nodes; ++i) {
        resolve_recursive(
            res_list, ast->nodes[i], ast, declaration_root,
            declarator_group, scope_level + is_scope_introduction
        );
    }

    if (ast->type == AST_PARAMETER && old_declaration_root->type == AST_FUNCTION_DEFINITION) {
        xcc_assert(ast->declaration);
        ast->declaration->decl_type = DECL_LOCAL_VAR;
    }

    if (is_scope_introduction) {
        res_list->num_local_declarations = old_num_locals;
    }

    if (ast->type == AST_FUNCTION_DEFINITION) {
        xcc_assert(ast->declaration);
        append_local_declaration_pointer(res_list, ast->declaration);
        res_list->current_func = NULL;
    }
}

ResolutionList *resolve_declarations(AST *program) {
    ResolutionList *res_list = xcc_malloc(sizeof(ResolutionList));

    res_list->all_declarations_head = NULL;
    res_list->current_func = NULL;
    res_list->local_declarations = NULL;
    res_list->num_local_declarations = 0;
    res_list->num_local_declarations_allocated = 0;

    resolve_recursive(res_list, program, NULL, NULL, NULL, 0);

    return res_list;
}

void resolve_free(ResolutionList *res) {
    Declaration *current = res->all_declarations_head;

    while (current) {
        Declaration *next = current->next_in_list;
        xcc_free(current);
        current = next;
    }
    xcc_free(res->local_declarations);
    xcc_free(res);
}

static const char *decl_type_to_str(DeclarationType t) {
    switch(t) {
        case DECL_LOCAL_VAR: return "DECL_LOCAL_VAR";
        case DECL_GLOBAL_VAR: return "DECL_GLOBAL_VAR";
        case DECL_FUNC_PROTOTYPE: return "DECL_FUNC_PROTOTYPE";
        case DECL_PARAM_TYPE: return "DECL_PARAM_TYPE";
    }
    xcc_assert_not_reached();
}

static void dump_declaration_list_implementation(Declaration *declaration) {
    if (declaration == NULL) return;

    dump_declaration_list_implementation(declaration->next_in_list);
    fprintf(
        stderr, "    [%p]: %s %s\n",
        declaration, declaration->name, decl_type_to_str(declaration->decl_type)
    );
}

void dump_declaration_list(ResolutionList *res_list) {
    fprintf(stderr, "Declaration list:\n");
    dump_declaration_list_implementation(res_list->all_declarations_head);
    fprintf(stderr, "\n");
}