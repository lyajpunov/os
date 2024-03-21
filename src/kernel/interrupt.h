// os/src/kernel/interrupt.h
#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H

#include "stdin.h"

#define PIC_M_CTRL 0x20	       // 这里用的可编程中断控制器是8259A,主片的控制端口是0x20
#define PIC_M_DATA 0x21	       // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0	       // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1	       // 从片的数据端口是0xa1

#define IDT_DESC_CNT 256     // 目前总共支持的中断数，我们直接一次弄齐

#define EFLAGS_IF 0x00000200   // eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

typedef void* intr_handler;

/* 中断门描述符结构体 */
struct gate_desc {
   uint16_t    func_offset_low_word;
   uint16_t    selector;
   uint8_t     dcount;         // 此项为双字计数字段，是门描述符中的第4字节。此项固定值，不用考虑
   uint8_t     attribute;
   uint16_t    func_offset_high_word;
}__attribute__((packed));      // 避免编译器优化，直接给对齐了

/* 定义中断的状态 */
enum intr_status {
    INTR_OFF,			    // 中断关闭
    INTR_ON		            // 中断打开
};

/* 获得当前中断状态 */
enum intr_status intr_get_status(void);
/* 设置当前中断状态 */
enum intr_status intr_set_status (enum intr_status);
/* 打开中断，并返回开中断之前的状态 */
enum intr_status intr_enable (void);
/* 关闭中断，并返回关中断之前的状态 */
enum intr_status intr_disable (void);
/* 初始化中断 */
void idt_init(void);
/* 在中断处理程序数组第vector_no个元素中注册安装中断处理程序function */
void register_handler(uint8_t vector_no, intr_handler function);


#endif