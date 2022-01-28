// @run!
// @rc: 0
// @run_output: 42

int supplement_print_i64(int x);

int main() {
    int xy = 41;
    supplement_print_i64(xy + 1);
}