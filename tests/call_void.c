// @run!
// @run_output: 22
// @rc: 3


void supplement_print_int(int x);
void supplement_print_nl();

void x() {
    supplement_print_int(2);
}

int main() {
    x();
    x();
    supplement_print_nl();
    return 3;
}
