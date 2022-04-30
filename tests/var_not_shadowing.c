// Regression test
// @compiles!
// @run!
// @rc: 42

int x() {
    int z = 5;
    z = z + 9;
    return z;
}

int main() {
    int z = x();
    z = z + x();
    return z + x();
}
