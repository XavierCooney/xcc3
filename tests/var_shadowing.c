// @compile_error!
// @xcc_msg: Program error: Redeclaration/shadowing of variable!
// @xcc_msg: `x`

int main() {
    int x;
    return 3;
    char x;
    return x;
}