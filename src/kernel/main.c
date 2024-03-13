// os/src/kernel/main.c

int main(void) {
    __asm__ __volatile__ ("movb $'M', %%gs:480" : : : "memory");
    __asm__ __volatile__ ("movb $'A', %%gs:482" : : : "memory");
    __asm__ __volatile__ ("movb $'I', %%gs:484" : : : "memory");
    __asm__ __volatile__ ("movb $'N', %%gs:486" : : : "memory");
    while (1) ;
    return 0;
}