#define NULL ((void*)0)
#include "stdint.h"



typedef float float32_t;

typedef struct
{
	float32_t*	in_x;
	float32_t*	in_h;
	float32_t*	out_y;
	uint32_t	len_x;
	uint32_t	len_h;
}f32_conv_struct;

void f32_conv(f32_conv_struct* in_struct);

void ftoa(float n, char *res, int afterpoint);
void toggle_buff(char* a, char* b);
void toggle_buff2(void* a, void* b);

void w2cpy(void* buff_h, void* buff_l, void* src, uint32_t len);
void memcpy_dw(void* dest, void* src, float factor, int n);

void conv2(float* X, size_t n1, float* H, size_t n2, float* Y);

uint32_t f32absmax(float* pSrc, float* pMaxVal, uint32_t blockSize);

uint32_t swap(uint32_t in);



#define f32abs(x)	\
({float __result, __arg = (x); \
__asm__ __volatile__ ( "vabs.f32 %[result], %[value]\n" \
			:[result] "=t" (__result) \
			:[value] "t" (__arg)); \
__result;})

// square root
#define hsqrtf(x)	\
({float __result, __arg = (x); \
__asm__ __volatile__ ( "vsqrt.f32 %[result], %[value]\n" \
			:[result] "=t" (__result) \
			:[value] "t" (__arg)); \
__result;})
// je¿eli uzywa siê rejestrów inline innych ni¿ wejœciowe to trzeba o tym poinformowaæ kompilator "clobber"
//ARM-GCC-Inline-Assembler-Cookbook.pdf
