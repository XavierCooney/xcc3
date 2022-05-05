// @run!
// @run_output_full: 89

void supplement_print_int(int x);
void supplement_print_nl();

int fib(int n) {
    if (n) {
        if (n - 1) {
            return fib(n - 1) + fib(n - 2);
        }
    }
    return 1;
}

int main() {
    supplement_print_int(fib(10));
}
