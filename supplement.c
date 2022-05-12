#include <stdio.h>

void supplement_print_int(int x) {
    printf("%d", x);
}
void supplement_print_space(int x) {
    printf(" ");
}
void supplement_print_char_int(int x) {
    printf("%c", (char) x);
}
void supplement_print_nl(void) {
    printf("\n");
}

int supplement_glob_1;

void set_glob_1(int x) {
    supplement_glob_1 = x;
}
int *get_glob_1_ptr(void) {
    return &supplement_glob_1;
}
