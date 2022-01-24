// @compile_error!
// @xcc_msg: Parse error!
// @xcc_msg: At "tests/int_func_name.c:6:5"
// @xcc_msg: Expected a IDENTIFIER, but found a KEYWORD_INT

int int() {
    return 1;
}