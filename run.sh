# os/run.sh
# 编译mbr
nasm  -I src/boot/ -o bin/mbr.bin src/boot/mbr.s 
nasm  -I src/boot/ -o bin/loader.bin src/boot/loader.s 

# 编译main.c
gcc -Wall -m32 -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes -o build/main.o src/kernel/main.c

# 链接
ld -m elf_i386 -T os.lds -e main build/main.o -o bin/kernel.bin 

# 复制mbr二进制程序到硬盘
dd if=bin/mbr.bin of=/home/lyj/bochs/bin/hd60M.img bs=512 count=1 seek=0 conv=notrunc
dd if=bin/loader.bin of=/home/lyj/bochs/bin/hd60M.img bs=512 count=8 seek=1 conv=notrunc
dd if=bin/kernel.bin of=/home/lyj/bochs/bin/hd60M.img bs=512 count=200 seek=10 conv=notrunc



# 启动仿真
/home/lyj/bochs/bin/bochs -f /home/lyj/bochs/bin/bochsrc.disk 

