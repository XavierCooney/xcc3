// @compile_error!
// @xcc_msg: Program error: shadowing of existing declaration!!
// @xcc_msg: `x`

int main() {
    int x;
    return 3;
    char x;
    return x;
}