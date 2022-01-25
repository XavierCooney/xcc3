#pragma once

#include "xcc.h"

typedef enum {
    POS_LITERAL, POS_STACK, POS_REG
} PositionType;


typedef struct ValuePosition {
    PositionType type;

    union {
        int stack_offset;
        int register_num;
    };
} ValuePosition;

void value_pos_allocate(AST *ast);
bool value_pos_is_same(ValuePosition *a, ValuePosition *b);
void value_pos_dump(ValuePosition *value_pos);