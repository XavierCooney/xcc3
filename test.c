// TODO: this should be INT_MAX, but types are needed first
// @xcc_msg: out of range integer literal
// @compile_error!

int main() {
    return 9223372039999999999999999996854775807;
}