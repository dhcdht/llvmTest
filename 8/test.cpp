#include <iostream>

extern "C" {
    double func(double, double);
}

int main() {
    std::cout << "func result of 3.0 and 4.0: " << func(3.0, 4.0) << std::endl;
}
