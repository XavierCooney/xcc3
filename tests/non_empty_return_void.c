// @compile_error!
// @xcc_msg: Cannot implicitly convert type!
// @xcc_msg: The expression has type int, but the expected type was void!

void my_func() {
    return 3;
}

int main() {
    my_func();
}
