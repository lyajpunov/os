; os/src/lib/kernel/print.s
TI_GDT equ  0
RPL0  equ   0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text
global put_char
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;打印单个字母
put_char:
    pushad	                    ; 备份32位寄存器环境
    mov ax, SELECTOR_VIDEO	    ; 不能直接把立即数送入段寄存器
    mov gs, ax                  ; 所以每次都要给视频段选择子赋值，避免错误

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;获取光标位置
    ;先获得高8位
    mov dx, 0x03d4              ;索引寄存器
    mov al, 0x0e	            ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x03d5              ;通过读写数据端口0x3d5来获得或设置光标位置 
    in al, dx	                ;得到了光标位置的高8位
    mov ah, al

    ;再获取低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5 
    in al, dx

    ;将光标存入bx
    mov bx, ax	  
    ;下面这行是在栈中获取待打印的字符
    mov ecx, [esp + 36]	      ;pushad压入4×8＝32字节,加上主调函数的返回地址4字节,故esp+36字节
    cmp cl, 0xd				  ;CR是0x0d,LF是0x0a
    jz .is_carriage_return
    cmp cl, 0xa
    jz .is_line_feed

    cmp cl, 0x8				  ;BS(backspace)的asc码是8
    jz .is_backspace
    jmp .put_other	   

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;判断是否为退格
.is_backspace:
    dec bx
    shl bx,1
    mov byte [gs:bx], 0x20		  ;将待删除的字节补为0或空格皆可
    inc bx
    mov byte [gs:bx], 0x07
    shr bx,1
    jmp .set_cursor

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;是ascii码
.put_other:
    shl bx, 1				     ; 光标位置是用2字节表示,将光标值乘2,表示对应显存中的偏移字节
    mov [gs:bx], cl			     ; ascii字符本身
    inc bx
    mov byte [gs:bx],0x07        ; 字符属性
    shr bx, 1				     ; 恢复老的光标值
    inc bx				         ; 下一个光标值
    cmp bx, 2000		   
    jl .set_cursor			     ; 若光标值小于2000,表示未写到显存的最后,则去设置新的光标值
					             ; 若超出屏幕字符数大小(2000)则换行处理
.is_line_feed:				     ; 是换行符LF(\n)
.is_carriage_return:		     ; 是回车符CR(\r)
    xor dx, dx				     ; dx是被除数的高16位,清0.
    mov ax, bx				     ; ax是被除数的低16位.
    mov si, 80				     ; 由于是效仿linux，linux中\n便表示下一行的行首，所以本系统中，
    div si				         ; 把\n和\r都处理为linux中\n的意思，也就是下一行的行首。
    sub bx, dx				     ; 光标值减去除80的余数便是取整

.is_carriage_return_end:		 ; 回车符CR处理结束
    add bx, 80
    cmp bx, 2000
.is_line_feed_end:			     ; 若是LF(\n),将光标移+80便可。  
    jl .set_cursor

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;滚动屏幕
.roll_screen:				     ; 若超出屏幕大小，开始滚屏
    ; mov [gs:320],'A'
    cld  
    mov ecx, 960				 ; 一共有2000-80=1920个字符要搬运,共1920*2=3840字节.一次搬4字节,共3840/4=960次 
    mov esi, 0xc00b80a0			 ; 第1行行首          # 需要特别注意这个地址，如果是内核程序，有这个映射，如果是用户程序，是没有这个低地址映射的
    mov edi, 0xc00b8000			 ; 第0行行首
    rep movsd				  
    mov ebx, 3840			     ; 最后一行填充为空白
    mov ecx, 80				     ; 一行是80字符(160字节),每次清空1字符(2字节),一行需要移动80次
.cls:
    mov word [gs:ebx], 0x0720    ;0x0720是黑底白字的空格键
    add ebx, 2
    loop .cls 
    mov bx,1920				     ;将光标值重置为1920,最后一行的首字符.

    ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;设置光标值为bx
.set_cursor:   
    mov dx, 0x03d4			     ;索引寄存器
    mov al, 0x0e			     ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x03d5			     ;通过读写数据端口0x3d5来获得或设置光标位置 
    mov al, bh
    out dx, al

    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5 
    mov al, bl
    out dx, al
    .put_char_done: 
    popad
    ret

global cls_screen
cls_screen:
    pushad
    ;;;;;;;;;;;;;;;
        ; 由于用户程序的cpl为3,显存段的dpl为0,故用于显存段的选择子gs在低于自己特权的环境中为0,
        ; 导致用户程序再次进入中断后,gs为0,故直接在put_str中每次都为gs赋值. 
    mov ax, SELECTOR_VIDEO	   ; 不能直接把立即数送入gs,须由ax中转
    mov gs, ax

    mov ebx, 0
    mov ecx, 80*25
.cls:
    mov word [gs:ebx], 0x0720 ;0x0720是黑底白字的空格键
    add ebx, 2
    loop .cls 
    mov ebx, 0

.set_cursor:			      ;直接把set_cursor搬过来用,省事
    ;;;;;;; 1 先设置高8位 ;;;;;;;;
    mov dx, 0x03d4			  ;索引寄存器
    mov al, 0x0e			  ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x03d5			  ;通过读写数据端口0x3d5来获得或设置光标位置 
    mov al, bh
    out dx, al

    ;;;;;;; 2 再设置低8位 ;;;;;;;;;
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5 
    mov al, bl
    out dx, al
    popad
    ret

; 全局的设置光标函数
global set_cursor
set_cursor:
   pushad
   mov bx, [esp+36]           ; 设置高8位
   mov dx, 0x03d4			  ; 索引寄存器
   mov al, 0x0e				  ; 用于提供光标位置的高8位
   out dx, al
   mov dx, 0x03d5			  ; 通过读写数据端口0x3d5来获得或设置光标位置 
   mov al, bh
   out dx, al
   mov dx, 0x03d4             ; 设置低8位
   mov al, 0x0f
   out dx, al
   mov dx, 0x03d5 
   mov al, bl
   out dx, al
   popad
   ret