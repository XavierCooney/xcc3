// @compile_error!
// @xcc_msg: redeclaration with incompatible types!

int foo(int x);
char foo(int x);

int main(void) { return 0; }