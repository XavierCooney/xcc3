// @compile_error!
// @xcc_msg: Cannot implicitly convert type:
// @xcc_msg: The expression has type [TYPE int], but the expected type was [TYPE void]!

void my_func() {
    return 3;
}

int main() {
    my_func();
}
