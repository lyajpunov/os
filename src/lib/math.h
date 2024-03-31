#ifndef __LIB_MATH_H
#define __LIB_MATH_H

#include "stdin.h"


#define do_mod(n, base)						\
({								\
	unsigned long __upper, __low, __high, __mod, __base;	\
	__base = (base);					\
	asm("" : "=a" (__low), "=d" (__high) : "A" (n));\
	__upper = __high;				\
	if (__high) {					\
		__upper = __high % (__base);		\
		__high = __high / (__base);	                	\
	}						                            \
	asm("divl %2" : "=a" (__low), "=d" (__mod)	        \
		: "rm" (__base), "0" (__low), "1" (__upper));	\
	asm("" : "=A" (n) : "a" (__low), "d" (__high));	    \
	__mod;							\
})


#define do_div(n, base)	({								\
	unsigned long __upper, __low, __high, __quotient, __base = (base);\
	asm("" : "=a" (__low), "=d" (__high) : "A" (n));\
	__upper = __high;				\
	if (__high) {					\
		__upper = __high % (__base);		\
		__high = __high / (__base);	                	\
	}						                            \
	asm("divl %2" : "=a" (__quotient), "=d" (__low)	        \
		: "rm" (__base), "0" (__low), "1" (__upper));	\
	asm("" : "=A" (n) : "a" (__low), "d" (__high));	    \
	__quotient;							\
})

uint64_t divide_u64_u32(uint64_t dividend, uint32_t divisor, uint64_t* remainder);
uint64_t divide_u64_u32_no_mod(uint64_t dividend, uint32_t divisor);
uint64_t mod_u64_u32(uint64_t dividend, uint32_t divisor);
uint64_t div_u64_rem(uint64_t dividend, uint32_t divisor, uint32_t* remainder);

#endif