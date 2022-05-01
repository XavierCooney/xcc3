// @run!
// @rc: 0
// @run_output: 42

int supplement_print_int(int x);

int main() {
    int xy = 41;
    supplement_print_int(xy + 1);
}