// @run!
// @rc: 7

int do_something_important(int x, int y);

int do_something_important(int x, int y) {
    return x + y;
}

int do_something_important(int a, int b);

int main(void) {
    return do_something_important(3, 4);
}