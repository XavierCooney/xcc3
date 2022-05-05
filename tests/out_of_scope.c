// @run!
// @run_output_full: 910

void supplement_print_int(int x);

int main() {
    if (1) {
        int z = 9;
        supplement_print_int(z);
    }

    if (0) {
        int z = 8;
        supplement_print_int(z);
    }

    if (1) {
        int z = 10;
        supplement_print_int(z);
    }
}
