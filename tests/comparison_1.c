// @run!
// @rc: 0
// @run_output_full: 1100033


void supplement_print_int(int x);
void supplement_print_nl();

int main() {
    int a = 0 < 1;
    int b = 1 >= 1;
    int c = 1 <= 0;
    int d = 1 < 1;
    int e = 1 > 1;
    supplement_print_int(a);
    supplement_print_int(b);
    supplement_print_int(c);
    supplement_print_int(d);
    supplement_print_int(e);

    if (1 > 2) {
        supplement_print_int(2);
    }
    if (1 > 1) {
        supplement_print_int(2);
    }
    if (2 > 1) {
        supplement_print_int(3);
    }

    if ((0 - 1) > 2) {
        supplement_print_int(2);
    }
    if ((0 - 1) > 1) {
        supplement_print_int(2);
    }
    if ((0 - 1) > (0 - 2)) {
        supplement_print_int(3);
    }


    return 1 < 0;
}
