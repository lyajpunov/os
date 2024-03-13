; os/src/boot/mbr.s
; 设置开始的地址，并且初始化寄存器
%include "boot.inc" 
SECTION MBR vstart=0x7c00         
    mov ax,cs      
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800
    mov gs,ax

; 利用0x06号功能实现清理屏幕
; AL = 0x06 功能号
; AL 上卷的行数(如果为0,表示全部)
; BH 上卷行属性
; (CL,CH) = 窗口左上角的(X,Y)位置,这里是 (0,0)
; (DL,DH) = 窗口右下角的(X,Y)位置,这里是 (80,25)
    mov    ah, 0x06
    mov    al, 0x00
    mov    bh, 0x7
    mov    bl, 0x00
    mov    cx, 0           
    mov    dx, 0x184f
    int    0x10             ; int 0x10

    mov byte [gs:0x00],'M'  ; 字符为M的ascii值
    mov byte [gs:0x01],0x0F	; 11100001b 即背景色为黑，字体为白，不闪烁 
    mov byte [gs:0x02],'B'  ;
    mov byte [gs:0x03],0x0F	; 
    mov byte [gs:0x04],'R'  ;
    mov byte [gs:0x05],0x0F	;

    mov eax,LOADER_START_SECTOR	 ; Loader起始扇区 
    mov bx, LOADER_BASE_ADDR     ; Loader起始内存地址
    mov cx, 8			         ; 待写入扇区数
    call rd_disk_m_16		     ; 执行读取硬盘程序
  
    jmp LOADER_BASE_ADDR         ; 跳转到Loader执行

rd_disk_m_16:	   
				       ; eax=LBA扇区号
				       ; ebx=Loader内存
				       ; ecx=扇区数量
    mov esi,eax	       ; 备份eax
    mov di,cx		   ; 备份cx

    mov dx,0x1f2       ; 设置要写入端口，即读取端口数
    mov al,cl          ; 设置要读取扇区数
    out dx,al          ; 设置
    mov eax,esi	       ; 恢复eax

    mov dx,0x1f3       ; 设置要写入端口，即LBA低地址              
    out dx,al          

    mov cl,8           ; ax右移八位   
    shr eax,cl
    mov dx,0x1f4       ; 设置要写入端口，即LBA中地址 
    out dx,al

    shr eax,cl         ; ax右移八位   
    mov dx,0x1f5       ; 设置要写入端口，即LBA高地址 
    out dx,al

    shr eax,cl         ; ax右移八位
    and al,0x0f	       ; 保留低4位，设置高4位为 0000
    or al,0xe0	       ; 保留低4位，设置高4位为 1110
    mov dx,0x1f6
    out dx,al

    mov dx,0x1f7       ;
    mov al,0x20        ; 读扇区指令               
    out dx,al


.not_ready:            ; 未准备好
    nop
    in al,dx           ; 查看读取状态
    and al,0x88        ; 与 10001000 做与运算
    cmp al,0x08        ; 比较第三位和第七位
    jnz .not_ready
    mov ax, di         ; 要读的扇区数
    mov dx, 256        ; 乘以256，即要读多少次
    mul dx
    mov cx, ax	       ; 将要读的次数传给cx
    mov dx, 0x1f0      ; 要读的端口号

.go_on_read:
    in ax,dx           ; 向ax中读，一次读两个字节
    mov [bx],ax        ; 将ax中数据给bx地址的内存
    add bx,2		   ; bx中内存地址加2
    loop .go_on_read   ; 循环cx次
    ret 
         
; 将510个字节中剩余的空间填充为0
; $ 是当前地址
; $$ 是本节开头地址，也就是0x7c00
times 510-($-$$) db 0
db 0x55,0xaa
