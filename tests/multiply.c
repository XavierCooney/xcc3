// @run!
// @rc: 0
// @run_output_full: 72\n72\n64\n72\n72\n-72\n72\n

void supplement_print_int(int x);
void supplement_print_nl();

int main() {
    int a = 8;
    int b = 9;

    supplement_print_int(8 * 9);
    supplement_print_nl();
    supplement_print_int(a * 9);
    supplement_print_nl();
    supplement_print_int(a * 8);
    supplement_print_nl();
    supplement_print_int(9 * a);
    supplement_print_nl();
    supplement_print_int(a * b);
    supplement_print_nl();
    supplement_print_int((0 - a) * b);
    supplement_print_nl();
    supplement_print_int((0 - a) * (0 - b));
    supplement_print_nl();
}
