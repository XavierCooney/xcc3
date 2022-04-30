// Regression test
// @compiles!
// @run!
// @rc: 42

char x() {
    char z = 5;
    z = z + 5;
    int y = 4;
    return z + y;
}

int main() {
    int z = x();
    z = z + x();
    return z + x();
}
