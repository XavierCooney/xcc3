// @run!
// @rc: 0
// @run_output_full: 4292

void supplement_print_int(int x);

int main() {
    if (1) if(0) supplement_print_int(8); else supplement_print_int(42);
    if (1) if(2) {
        supplement_print_int(9);
        supplement_print_int(2);
    }
    if (0) if(2) supplement_print_int(10); else { supplement_print_int(13); }
    if (0) if(0) { supplement_print_int(11); } else supplement_print_int(12);
}
