AS = nasm
CC = gcc
LD = ld
ACFLAG = -Wall -m32 -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes 
ASFLAG = -I src/boot/
LDFLAG = -m elf_i386 -e main -Ttext 0xc0001500 -T os.lds

###################################################################################### 头文件
INCUDIRS  := src/lib \
			 src/lib/kernel \
			 src/device \
			 src/kernel
INCLUDE   := $(patsubst %, -I %, $(INCUDIRS))

###################################################################################### C文件与S文件的编译
SRCDIRS   := src/kernel \
			 src/lib \
			 src/lib/kernel \
			 src/device

SFILES    := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.s))
CFILES    := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))
SFILENDIR := $(notdir $(SFILES))
CFILENDIR := $(notdir $(CFILES))
SOBJS     := $(patsubst %.s, build/%.o, $(SFILENDIR))
COBJS     := $(patsubst %.c, build/%.o, $(CFILENDIR))
OBJS 	  := $(COBJS) $(SOBJS) 

VPATH     := $(SRCDIRS)

$(SOBJS):build/%.o:%.s
	$(AS) -f elf $(ASFLAG) $(INCLUDE) $< -o $@
$(COBJS):build/%.o:%.c
	$(CC) $(ACFLAG) $(INCLUDE) $< -o $@

###################################################################################### 单独编译代码启动
bin/mbr.bin: src/boot/mbr.s
	$(AS) $(ASFLAG) $< -o $@
bin/loader.bin: src/boot/loader.s
	$(AS) $(ASFLAG) $< -o $@

###################################################################################### 链接代码 # $(LD) $^ $(LDFLAGS) -o $@
bin/kernel.bin:$(OBJS)
	$(LD) $(LDFLAG) $^ -o $@
	

.PHONY: mk_dir clear copy
# 查看编译缓存的文件是否存在，存在就继续，不存在则创建
mk_dir:
	if [ ! -d build ];then mkdir build;fi
	if [ ! -d bin ];then mkdir bin;fi
# 删除编译缓存的文件
clear:
	rm -rf ./build/*
	rm -rf ./bin/*
# 复制二进制程序
copy:
	dd if=bin/mbr.bin of=/home/lyj/bochs/bin/hd60M.img bs=512 count=1 seek=0 conv=notrunc
	dd if=bin/loader.bin of=/home/lyj/bochs/bin/hd60M.img bs=512 count=8 seek=1 conv=notrunc
	dd if=bin/kernel.bin of=/home/lyj/bochs/bin/hd60M.img bs=512 count=200 seek=10 conv=notrunc
# 启动仿真
begin:
	/home/lyj/bochs/bin/bochs -f /home/lyj/bochs/bin/bochsrc.disk 
# 编译程序
build: bin/mbr.bin bin/loader.bin bin/kernel.bin
# 启动所有程序
all: mk_dir clear build copy begin

print:
	@echo $(SOBJS)
	@echo $(COBJS)
	@echo $(OBJS)

