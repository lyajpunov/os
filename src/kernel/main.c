// os/src/kernel/main.c

#include "print.h"
#include "init.h"

int main(void) {
    init_all();

    while (1);
    return 0;
}