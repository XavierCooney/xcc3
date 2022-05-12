// @compile_error!
// @xcc_msg: can't have an initialiser for parameter!

int foo(int x = 2) {
    return 2;
}

int main(void) {
    return 0;
}