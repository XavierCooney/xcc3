#pragma once

#include "xcc.h"

typedef enum {
    POS_LITERAL, POS_STACK, POS_REG, POS_VOID
} PositionType;

typedef enum {
    REG_RAX, REG_RBX, REG_RCX, REG_RDX, REG_RSP, REG_RBP, REG_RSI,
    REG_RDI, REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13,
    REG_R14, REG_R15,

    REG_LAST
} RegLoc;


typedef struct ValuePosition {
    PositionType type;

    union {
        int stack_offset;
        RegLoc register_num;
    };

    int size;
    int alignment;
    bool is_signed;
} ValuePosition;

void value_pos_allocate(AST *ast);
bool value_pos_is_same(ValuePosition *a, ValuePosition *b);
ValuePosition *value_pos_reg(RegLoc location, int reg_size);
void value_pos_free_preallocated();
void value_pos_dump(ValuePosition *value_pos);
