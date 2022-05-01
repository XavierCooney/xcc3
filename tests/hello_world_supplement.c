// @run!
// @rc: 0
// @run_output: 12 A
// @run_output: Hello, World!

int supplement_print_int(int x);
int supplement_print_char_int(int c);
int supplement_print_nl();
int supplement_print_space();

int main() {
    supplement_print_int(12);
    supplement_print_space();
    supplement_print_char_int(65); // 'A'
    supplement_print_nl();

    // "Hello, World!\n"
    supplement_print_char_int(72);
    supplement_print_char_int(101);
    supplement_print_char_int(108);
    supplement_print_char_int(108);
    supplement_print_char_int(111);
    supplement_print_char_int(44);
    supplement_print_char_int(32);
    supplement_print_char_int(87);
    supplement_print_char_int(111);
    supplement_print_char_int(114);
    supplement_print_char_int(108);
    supplement_print_char_int(100);
    supplement_print_char_int(33);
    supplement_print_char_int(10);
}