#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void *xcc_malloc(size_t size);
void xcc_free(const void *p);

#define NORETURN __attribute__((__noreturn__))

#include "lexer.h"

void begin_prog_error_range(const char *msg, Token *start_token, Token *end_token);
NORETURN void end_prog_error();
#define prog_error(msg, token) do { begin_prog_error_range((msg), (token), (token)); end_prog_error(); } while(false);

#include "xcc_assert.h"
#include "list.h"
#include "ast.h"
#include "parser.h"
