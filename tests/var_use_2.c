// @run!
// @rc: 0
// @run_output: 28
// @run_output: 169

int supplement_print_int(int x);
int supplement_print_nl();

int main() {
    int a = 4;
    int b = 9;
    int c = 20;
    int d = 50;
    supplement_print_int(a + c + a);
    supplement_print_nl();
    int e = 99;
    int f = b + b + b + a + a;
    supplement_print_int(f + f + e);
}