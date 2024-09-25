#include <stdio.h>
#include <stdlib.h>

unsigned long long fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(int argc, char *argv[]) {
    int i = atoi(argv[1]); 
    printf("%llu ", fibonacci(i));

    return 0;
}

