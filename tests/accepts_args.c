// @run!
// @rc: 6
// @run_output: 42

void supplement_print_int(int x);

int my_func(int z, int y) {
    supplement_print_int(y);
    supplement_print_int(z);
    return y + z;
}

int main() {
    return my_func(2, 4);
}
