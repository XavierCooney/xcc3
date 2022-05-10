// @compile_error!
// @xcc_msg: redefinition of function!

int foo(int x) {
    return 2;
}

int foo(int x) {
    return 2;
}

int main(void) {
    return 3;
}