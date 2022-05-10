// @compile_error!
// @xcc_msg: cannot initialise with this declaration!

int g(int x);

int main(void) {
    int foo(int y) = g;
    return 0;
}