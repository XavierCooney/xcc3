// @run!
// @rc: 7

int set_glob_1(int x);
int *get_glob_1_ptr(void);

int main() {
    int *x = get_glob_1_ptr();
    set_glob_1(3);
    int y = *x;
    set_glob_1(4);

    return y + *x;
}
