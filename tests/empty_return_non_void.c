// @compile_error!
// @xcc_msg: empty return in non-void function!

int my_func() {
    return;
}

int main() {
    return my_func();
}
