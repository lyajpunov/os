// os/src/kernel/main.c

#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"

void k_thread_a(void*);
void k_thread_b(void*);

int main(void) {
    init_all();
    intr_enable();
    put_str("I am kernel\n");
    thread_start("k_thread_a", 5, k_thread_a, "argA ");
    thread_start("k_thread_b", 5, k_thread_b, "argB "); 
    while (1);
    return 0;
}

void k_thread_a(void* arg) {
    char* para = arg;
    int i = 0;
    while (1) {
        if (i % 1000 == 0) { put_str(para); i = 1; }
        i++;
    }
}

void k_thread_b(void* arg) {
    char* para = arg;
    int i = 0;
    while (1) {
        if (i % 1000 == 0) { put_str(para); i = 1; }
        i++;
    }
}
