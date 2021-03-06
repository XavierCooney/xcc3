#pragma once

#include "xcc.h"

typedef enum {
    TYPE_VOID, TYPE_INTEGER, TYPE_ENUM, TYPE_ARRAY,
    TYPE_STRUCT, TYPE_UNION, TYPE_POINTER, TYPE_FUNCTION,
    // TYPE_FLOAT
} TypeType;
// TODO: TYPE_ENUM should be in TypeInteger

typedef enum {
    TYPE_CHAR,
    TYPE_SCHAR, TYPE_SHORT, TYPE_INT, TYPE_LONG, TYPE_LONG_LONG,
    TYPE_BOOL, TYPE_UCHAR, TYPE_USHORT, TYPE_UINT, TYPE_ULONG, TYPE_ULONG_LONG,
    TYPE_INTEGER_LAST
} TypeInteger;

typedef struct Type {
    TypeType type_type; // type type type type type
    TypeInteger integer_type;

    bool is_const;
    bool is_volatile;
    bool is_restrict;
    int array_size; // -1 for unknown bounds

    struct Type *underlying;
    struct Type **function_param_types;
    struct Type *next_type;
} Type;

Type *type_new_int(TypeInteger integer_type, bool is_const, bool is_volatile);
bool integer_type_is_signed(Type *type);
void type_propogate(AST *ast);
void type_free_all();
void type_dump(Type *type);
