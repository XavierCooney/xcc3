// @run!
// @rc: 0
// @run_output_full: 41 20 30 47 47 30

int supplement_print_i64(int x);
int supplement_print_space();

int main() {
    int x = 41;
    int y = 20;
    int z = 30;
    supplement_print_i64(x);
    supplement_print_space();
    supplement_print_i64(y);
    supplement_print_space();
    supplement_print_i64(z);
    supplement_print_space();
    x = y = 47;
    supplement_print_i64(x);
    supplement_print_space();
    supplement_print_i64(y);
    supplement_print_space();
    supplement_print_i64(z);
}