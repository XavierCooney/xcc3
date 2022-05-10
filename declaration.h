#pragma once

#include "xcc.h"

typedef enum {
    DECL_LOCAL_VAR, DECL_GLOBAL_VAR,
    DECL_FUNC_PROTOTYPE, DECL_PARAM_TYPE
} DeclarationType;

struct ValuePosition;
typedef struct Declaration {
    const char *name;
    struct Type *type;
    DeclarationType decl_type;
    AST *last_declaration_ast;
    ValuePosition *pos;
    AST *definition_ast;
    int scope_level;

    struct Declaration *next_in_list;
} Declaration;

typedef struct {
    Declaration *all_declarations_head;

    int num_local_declarations;
    int num_local_declarations_allocated;
    Declaration **local_declarations;

    AST *current_func;
} ResolutionList;

ResolutionList *resolve_declarations(AST *program);
void resolve_free(ResolutionList *res);
void dump_declaration_list(ResolutionList *res_list);