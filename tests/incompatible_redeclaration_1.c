// @compile_error!
// @xcc_msg: redeclaration with incompatible types!

int foo(int x, int y, int z);
int foo(int x, char y, int z);

int main(void) { return 0; }