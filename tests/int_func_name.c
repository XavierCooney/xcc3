// @compile_error!
// @xcc_msg: Parse error
// @xcc_msg: Near "tests/int_func_name.c:6:9"
// @xcc_msg: expected declarator

int int() {
    return 1;
}