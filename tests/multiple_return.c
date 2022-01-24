// @run!
// @rc: 3

int func() {
    return 2;
    return 5;
    return 7;
}

int main() {
    return 3;
    return 0;
}

int thing() {
    return 1;
    return 100;
}