#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


void *xcc_malloc(size_t size);
void xcc_free(const void *p);

#include "xcc_assert.h"
#include "list.h"
#include "lexer.h"
