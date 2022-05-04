// @run!
// @run_output: -3
// @rc: 4

int supplement_print_int(int x);

int main() {
    int x = 2 - 5;
    supplement_print_int(x);

    return 1 - x;
}