// @compile_error!
// @xcc_msg: Cannot implicitly convert type!
// @xcc_msg: The expression has type void, but the expected type was int!

void do_something() {}

int main() {
    int x = do_something();
    return x;
}