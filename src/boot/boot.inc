; os/src/boot/boot.inc
LOADER_BASE_ADDR equ 0x600
LOADER_START_SECTOR equ 0x1


;--------------   gdt描述符属性  -------------
DESC_G_4K   equ	  1_00000000000000000000000b    ; 段界限单位：4KB
DESC_D_32   equ	   1_0000000000000000000000b    ; 有效地址和操作数是 32 位
DESC_L	    equ	    0_000000000000000000000b	; 64位代码标记，此处标记为0便可。
DESC_AVL    equ	     0_00000000000000000000b	; 保留位
DESC_LIMIT_CODE2  equ 1111_0000000000000000b    ; 代码段段界限的高位
DESC_LIMIT_DATA2  equ 1111_0000000000000000b    ; 数据段段界限的高位
DESC_LIMIT_VIDEO2 equ 0000_0000000000000000b    ; 视频段段界限的高位
DESC_P	    equ		  1_000000000000000b        ; 段是否存在标志位
DESC_DPL_0  equ		   00_0000000000000b        ; 0特权级内存
DESC_DPL_1  equ		   01_0000000000000b        ; 1特权级内存
DESC_DPL_2  equ		   10_0000000000000b        ; 2特权级内存
DESC_DPL_3  equ		   11_0000000000000b        ; 3特权级内存
DESC_S_CODE equ		     1_000000000000b        ; 代码段的段描述为数据段 
DESC_S_DATA equ	         1_000000000000b        ; 数据段的段描述为数据段 
DESC_S_sys  equ		     0_000000000000b        ; 系统段的段描述为系统段  
DESC_TYPE_CODE  equ	      1000_00000000b	;x=1,c=0,r=0,a=0 代码段是可执行的,非依从的,不可读的,已访问位a清0.  
DESC_TYPE_DATA  equ	      0010_00000000b	;x=0,e=0,w=1,a=0 数据段是不可执行的,向上扩展的,可写的,已访问位a清0.

DESC_CODE_HIGH4  equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0x00
DESC_DATA_HIGH4  equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00
DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b

;--------------   选择子属性  ---------------
RPL0  equ   00b
RPL1  equ   01b
RPL2  equ   10b
RPL3  equ   11b
TI_GDT	 equ   000b
TI_LDT	 equ   100b
;--------------   页表属性  ---------------
PAGE_DIR_TABLE_POS equ 0x100000
PG_P  equ   1b
PG_RW_R	 equ  00b 
PG_RW_W	 equ  10b 
PG_US_S	 equ  000b 
PG_US_U	 equ  100b 
