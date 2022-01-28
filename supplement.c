#include <stdio.h>

typedef long long i64;

void supplement_print_i64(i64 x) {
    printf("%lli", x);
}
void supplement_print_space(i64 x) {
    printf(" ");
}
void supplement_print_char_i64(i64 x) {
    printf("%c", (char) x);
}
void supplement_print_nl() {
    printf("\n");
}