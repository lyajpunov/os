// os/src/kernel/main.c

#include "init.h"
#include "thread.h"
#include "console.h"
#include "interrupt.h"
#include "keyboard.h"
#include "ioqueue.h"

void k_thread_a(void*);
void k_thread_b(void*);


int main(void) {
    init_all();
    intr_enable();
    console_put_str("I am kernel\n");
    // thread_start("k_thread_a", k_thread_a, "argA ");
    // thread_start("k_thread_b", k_thread_b, "argB "); 
    while (1);
    return 0;
}

void k_thread_a(void* arg) {
    char* para = arg;
    int i = 0;
    while (1) {
        console_put_char(ioq_getchar(&kbd_buf));
    }
}

void k_thread_b(void* arg) {
    char* para = arg;
    int i = 0;
    while (1) {
        if (i % 1000 == 0) { console_put_str(para); i = 1; }
        i++;
    }
}
