// @run!
// @run_output_full: 9

void supplement_print_int(int x);

int main() {
    if (0) if(2) supplement_print_int(8);
    if (1) if(2) supplement_print_int(9);
    if (0) if(2) supplement_print_int(10);
    if (0) if(0) supplement_print_int(11);
}
