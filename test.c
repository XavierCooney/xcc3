int supplement_print_i64(int x);
int supplement_print_char_i64(int c);
int supplement_print_nl();
int supplement_print_space();

int main() {
    supplement_print_i64(12);
    supplement_print_space();
    supplement_print_char_i64(65); // 'A'
    supplement_print_nl();

    // "Hello, World!\n"
    supplement_print_char_i64(72);
    supplement_print_char_i64(101);
    supplement_print_char_i64(108);
    supplement_print_char_i64(108);
    supplement_print_char_i64(111);
    supplement_print_char_i64(44);
    supplement_print_char_i64(32);
    supplement_print_char_i64(87);
    supplement_print_char_i64(111);
    supplement_print_char_i64(114);
    supplement_print_char_i64(108);
    supplement_print_char_i64(100);
    supplement_print_char_i64(33);
    supplement_print_char_i64(10);
}