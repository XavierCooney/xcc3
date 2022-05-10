// @compile_error!
// @xcc_msg: expected lvalue to assign to!

int g(int x);

int main(void) {
    int foo(int y);
    foo = g;
    return 0;
}