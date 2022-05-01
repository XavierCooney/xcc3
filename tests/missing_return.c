// @compile_error!
// @xcc_msg: function doesn't have a return:

int g() {
    return 3;
}

int f() {
    int z = g() + g();
}

int main() {
    return g();
}
