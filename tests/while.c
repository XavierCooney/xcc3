// @run!
// @run_output_full: 35792

void supplement_print_int(int x);
void supplement_print_nl();

int main() {
    int i = 3;

    while (i < 11) {
        supplement_print_int(i);
        // supplement_print_nl();
        int new_i = i + 1;
        i = new_i + 1;
    }

    int j = 6;
    while (j > 8) {
        supplement_print_int(i);
        i = i - 1;
    }
    supplement_print_int(2);
}