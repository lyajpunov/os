// os/src/kernel/main.c

#include <print.h>

int main(void) {
    put_char('M');
    put_char('A');
    put_char('I');
    put_char('N');
    put_char('!');
    put_char('\n');
    put_str("12345\n");
    put_int(123456780);
    put_char('\n');
    put_hex(12345678);
    while (1) ;
    return 0;
}