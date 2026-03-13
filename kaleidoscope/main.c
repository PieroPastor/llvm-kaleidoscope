#include "stdio.h"

extern double average(double, double);
extern double addition(double, double);
extern double addone(double);

int main() {
    printf("average of 3.0 and 4.0: %f\n", average(3.0, 4.0));
    printf("addition of 3 and 4: %f\n", addition(3, 4.0));
    printf("addition of 3 and 1: %f\n", addone(3));
    return 0;
}