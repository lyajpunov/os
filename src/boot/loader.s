; os/src/boot/loader.s
%include "boot.inc" 
section loader vstart=LOADER_BASE_ADDR ; 程序开始的地址

jmp loader_start

LOADER_STACK_TOP equ LOADER_BASE_ADDR ; 栈顶地址

;构建gdt及其内部的描述符
GDT_BASE:  dd    0x00000000 
	       dd    0x00000000

CODE_DESC: dd    0x0000FFFF 
	       dd    DESC_CODE_HIGH4

DATA_STACK_DESC:  dd    0x0000FFFF
		          dd    DESC_DATA_HIGH4

VIDEO_DESC: dd    0x80000007	       ; limit=(0xbffff-0xb8000)/4k=0x7
	        dd    DESC_VIDEO_HIGH4     ; 此时dpl为0

GDT_SIZE   equ   $ - GDT_BASE
GDT_LIMIT  equ   GDT_SIZE -	1 
times 60 dq 0					 ; 此处预留60个描述符的slot
SELECTOR_CODE  equ (0x0001<<3) + TI_GDT + RPL0   ; 第一个选择子
SELECTOR_DATA  equ (0x0002<<3) + TI_GDT + RPL0	 ; 第二个选择子
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0	 ; 第三个选择子

; 以下是定义gdt的指针，前2字节是gdt界限，后4字节是gdt起始地址
gdt_ptr  dw  GDT_LIMIT 
	     dd  GDT_BASE

total_mem_bytes dd 0		; 保存内存容量，以字节为单位
ards_buf times 244 db 0     ; 人工对齐:total_mem_bytes4字节+gdt_ptr6字节+ards_buf244字节+ards_nr2,共256字节
ards_nr dw 0		        ; 用于记录ards结构体数量

loader_start:
    mov byte [gs:160],'L'
    mov byte [gs:161],0x0F
    mov byte [gs:162],'O'
    mov byte [gs:163],0x0F
    mov byte [gs:164],'A'
    mov byte [gs:165],0x0F   
    mov byte [gs:166],'D'
    mov byte [gs:167],0x0F
    mov byte [gs:168],'E'
    mov byte [gs:169],0x0F
    mov byte [gs:170],'R'
    mov byte [gs:171],0x0F

; 获取内存容量，int 15, ax = E820h
.get_total_mem_bytes:
    xor ebx, ebx              ;第一次调用时，ebx值要为0
    mov edx, 0x534d4150	      ;edx只赋值一次，循环体中不会改变
    mov di, ards_buf	      ;ards结构缓冲区
.e820_mem_get_loop:	          ;循环获取每个ARDS内存范围描述结构
    mov eax, 0x0000e820	      ;执行int 0x15后,eax值变为0x534d4150,所以每次执行int前都要更新为子功能号。
    mov ecx, 20		          ;ARDS地址范围描述符结构大小是20字节
    int 0x15
    jc .failed_so_try_e801    ;若cf位为1则有错误发生，尝试0xe801子功能
    add di, cx		          ;使di增加20字节指向缓冲区中新的ARDS结构位置
    inc word [ards_nr]	      ;记录ARDS数量
    cmp ebx, 0		          ;若ebx为0且cf不为1,这说明ards全部返回，当前已是最后一个
    jnz .e820_mem_get_loop

    ;在所有ards结构中，找出(base_add_low + length_low)的最大值，即内存的容量。
    mov cx, [ards_nr]	      ;遍历每一个ARDS结构体,循环次数是ARDS的数量
    mov ebx, ards_buf 
    xor edx, edx		      ;edx为最大的内存容量,在此先清0
.find_max_mem_area:	          ;无须判断type是否为1,最大的内存块一定是可被使用
    mov eax, [ebx]	          ;base_add_low
    add eax, [ebx+8]	      ;length_low
    add ebx, 20		          ;指向缓冲区中下一个ARDS结构
    cmp edx, eax		      ;冒泡排序，找出最大,edx寄存器始终是最大的内存容量
    jge .next_ards
    mov edx, eax		      ;edx为总内存大小
.next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok

; 获取内存容量，int 15, ax = E801h
.failed_so_try_e801:
    mov ax,0xe801
    int 0x15
    jc .failed_so_try88       ;若当前e801方法失败,就尝试0x88方法

    ; 先算出低15M的内存,ax和cx中是以KB为单位的内存数量,将其转换为以byte为单位
    mov cx,0x400	          ;cx和ax值一样,cx用做乘数
    mul cx 
    shl edx,16
    and eax,0x0000FFFF
    or edx,eax
    add edx, 0x100000         ;ax只是15MB,故要加1MB
    mov esi,edx	              ;先把低15MB的内存容量存入esi寄存器备份

    ; 再将16MB以上的内存转换为byte为单位,寄存器bx和dx中是以64KB为单位的内存数量
    xor eax,eax
    mov ax,bx		
    mov ecx, 0x10000	      ;0x10000十进制为64KB
    mul ecx		              ;32位乘法,默认的被乘数是eax,积为64位,高32位存入edx,低32位存入eax.
    add esi,eax		          ;由于此方法只能测出4G以内的内存,故32位eax足够了,edx肯定为0,只加eax便可
    mov edx,esi		          ;edx为总内存大小
    jmp .mem_get_ok

; 获取内存容量，int 15, ah = 0x88
.failed_so_try88: 
    ;int 15后，ax存入的是以kb为单位的内存容量
    mov ah, 0x88
    int 0x15
    jc  .error_hlt
    and eax,0x0000FFFF
      
    ;16位乘法，被乘数是ax,积为32位.积的高16位在dx中，积的低16位在ax中
    mov cx, 0x400      ;0x400等于1024,将ax中的内存容量换为以byte为单位
    mul cx
    shl edx, 16	       ;把dx移到高16位
    or  edx, eax	   ;把积的低16位组合到edx,为32位的积
    add edx,0x100000   ;0x88子功能只会返回1MB以上的内存,故实际内存大小要加上1MB
    jmp .mem_get_ok

;将内存换为byte单位后存入total_mem_bytes处。
.mem_get_ok:
    mov [total_mem_bytes], edx	 

; 打开A20地址线
.open_A20:
    in   al,0x92
    or   al,0000_0010B
    out  0x92,al

; 加载gdt描述符
.load_gdt:
    lgdt [gdt_ptr]

; 修改cr0标志寄存器的PE位
.change_cr0_PE:
    mov  eax, cr0
    or   eax, 0x00000001
    mov  cr0, eax

.jmp_bit_32
    jmp  SELECTOR_CODE:p_mode_start ; 刷新流水线，避免分支预测的影响
					                ; 远跳将导致之前做的预测失效，从而起到了刷新的作用。

.error_hlt:		      ;出错则挂起
    hlt

; 下面就是保护模式下的程序了
[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp,LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov byte [gs:320], 'M'
    mov byte [gs:322], 'A'
    mov byte [gs:324], 'I'
    mov byte [gs:326], 'N'

    call setup_page ; 创建页目录及页表并初始化页内存位图

    ;要将描述符表地址及偏移量写入内存gdt_ptr,一会用新地址重新加载
    sgdt [gdt_ptr]	      ; 存储到原来gdt的位置

    ;将gdt描述符中视频段描述符中的段基址+0xc0000000
    mov ebx, [gdt_ptr + 2]  
    or dword [ebx + 0x18 + 4], 0xc0000000      ;视频段是第3个段描述符,每个描述符是8字节,故0x18。
					                           ;段描述符的高4字节的最高位是段基址的31~24位

    ;将gdt的基址加上0xc0000000使其成为内核所在的高地址
    add dword [gdt_ptr + 2], 0xc0000000

    add esp, 0xc0000000        ; 将栈指针同样映射到内核地址

    ; 把页目录地址赋给cr3
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

    ; 打开cr0的pg位(第31位)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ;在开启分页后,用gdt新的地址重新加载
    lgdt [gdt_ptr]             ; 重新加载

    mov byte [gs:320], 'V'     ;视频段段基址已经被更新,用字符v表示virtual addr
    mov byte [gs:322], 'i'     ;视频段段基址已经被更新,用字符v表示virtual addr
    mov byte [gs:324], 'r'     ;视频段段基址已经被更新,用字符v表示virtual addr
    mov byte [gs:326], 't'     ;视频段段基址已经被更新,用字符v表示virtual addr
    mov byte [gs:328], 'u'     ;视频段段基址已经被更新,用字符v表示virtual addr
    mov byte [gs:330], 'a'     ;视频段段基址已经被更新,用字符v表示virtual addr
    mov byte [gs:332], 'l'     ;视频段段基址已经被更新,用字符v表示virtual addr

    jmp $

setup_page:                      ; 创建页目录及页表
    mov ecx, 4096
    mov esi, 0
.clear_page_dir:                 ; 清理页目录空间
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

.create_pde:				         ; 创建页目录
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000 			     ; 此时eax为第一个页表的位置及属性,属性全为0
    mov ebx, eax				     ; 此处为ebx赋值，是为.create_pte做准备，ebx为基址。

    ;   下面将页目录项0和0xc00都存为第一个页表的地址，
    ;   一个页表可表示4MB内存,这样0xc03fffff以下的地址和0x003fffff以下的地址都指向相同的页表，
    ;   这是为将地址映射为内核地址做准备
    or eax, PG_US_U | PG_RW_W | PG_P	      ; 页目录项的属性RW和P位为1,US为1,表示用户属性,所有特权级别都可以访问.
    mov [PAGE_DIR_TABLE_POS + 0x0], eax       ; 第1个目录项,在页目录表中的第1个目录项写入第一个页表的位置(0x101000)及属性(7)
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax     ; 一个页表项占用4字节,0xc00表示第768个页表占用的目录项,0xc00以上的目录项用于内核空间,
					                          ; 也就是页表的0xc0000000~0xffffffff共计1G属于内核,0x0~0xbfffffff共计3G属于用户进程.
    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_POS + 4092], eax	  ; 使最后一个目录项指向页目录表自己的地址

;下面创建第一个页表PTE，其地址为0x101000，也就是1MB+4KB的位置，需要映射前1MB内存
    mov ecx, 256				              ; 1M低端内存 / 每页大小4k = 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P	      ; 属性为7,US=1,RW=1,P=1
.create_pte:
    mov [ebx+esi*4],edx			              ; 此时的ebx已经在上面成为了第一个页表的地址，edx地址为0，属性为7
    add edx,4096                              ; edx+4KB地址
    inc esi                                   ; 循环256次
    loop .create_pte

;创建内核其它页表的PDE
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000 		                     ; 此时eax为第二个页表的位置
    or eax, PG_US_U | PG_RW_W | PG_P             ; 页目录项的属性为7
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254			                     ; 范围为第769~1022的所有目录项数量
    mov esi, 769
.create_kernel_pde:
    mov [ebx+esi*4], eax
    inc esi
    add eax, 0x1000
    loop .create_kernel_pde
    ret