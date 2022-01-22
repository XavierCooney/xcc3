parts = xcc lexer

object_files = $(addsuffix .o,$(addprefix build/,$(parts)))
source_files = $(addsuffix .c,$(parts))
header_files = *.h
cflags = -fsanitize=undefined -Wall -Werror -ggdb -Wno-format-zero-length

.PHONY: all
all: xcc

.PHONY: run
run: xcc
	./xcc

.PHONY: debug
debug: xcc
	gdb ./xcc

.PHONY: find_leak
find_leak: xcc
	valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
		 ./xcc test.c -o out.S -v

.PHONY: clean
clean:
	rm -r build/* xcc || true

build/:
	mkdir build/

xcc: $(object_files)
	gcc $(object_files) -o xcc $(cflags)

build/%.o: %.c $(header_files)
	gcc $< -o $@ -c $(cflags)