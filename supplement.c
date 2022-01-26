#include <stdio.h>

typedef long long i64;

i64 supplement_print_i64(i64 x) {
    printf("%lli", x);
}
i64 supplement_print_space(i64 x) {
    printf(" ");
}
i64 supplement_print_char_i64(i64 x) {
    printf("%c", (char) x);
}
i64 supplement_print_nl() {
    printf("\n");
}