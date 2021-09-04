#include <stdio.h>
#include <math.h>
#include "stdint.h"

//#define hhsqrtf(X) __asm__ volatile ("vsqrt.f32 %[result], %[input]" : : "r" (data), "r" (dest_addr))

#include "helper.h"

void f32_conv(f32_conv_struct* in_struct)
{
	__asm(
			"push {r4-r7}\n"
			"ldr r1, [r0,#12]\n"	// len x
			"ldr r2, [r0,#4]\n"		// h
			"ldr r3, [r0,#16]\n"	// len h
			"ldr r4, [r0,#8]\n"		// y
			"ldr r0, [r0]\n"		// x

			"loopx2:\n"
			"mov r6, r3\n"				// ilosc iteracji petli H
			"mov r7, r2\n"				// pozycja H
			"mov r5, r4\n"				// pozycja Y

			"vldr.f32 s0, [r0]\n"		// read current X val

			"looph2:\n"
			"vldr.f32 s1, [r7]\n"		// read current H val
			"vldr.f32 s2, [r5]\n"		// read current Y val
			"vmla.f32 s2, s0, s1\n"		// mnozenie i sumowanie
			"vstr.f32 s2, [r5]\n"		// zapisanie wyniku w Y

			"add r7, r7, #4\n"		    // increment current H pos
			"add r5, r5, #4\n"			// increment current Y pos

			"subs r6, r6, #1\n"
			"bne looph2\n"

			"add r0, r0, #4\n"			//incrase input pos
			"add r4, r4, #4\n"			//incrase output pos
			"subs r1, r1, #1\n"			// external loop decr

			"bne loopx2\n"

			"pop {r4-r7}\n"
		);
}

// kopiuje podwojne sowo
void memcpy_dw(void* dest, void* src, float factor, int n)
{
	__asm(
			"push {r4}\n"
			"loopx:\n"
			"ldr r4, [r1], #4\n"   		// odczytujemy wartosc spod adresu r1
			"vmov.f32 s1, r4\n"
			"vmul.f32 s1, s1, s0\n"
			"vmov.f32 r4, s1\n"
			"str r4, [r0], #4\n"	// zapis z lbuff do hbuff i inkrementaca hbuff
			"subs r2,#1\n"
			"bne loopx\n"
			"pop {r4}\n"
			);
}

//kopiuje n=len slow z *r1 do *r0, i *r2 do *r1
//r0-dst1, r1-dst2, r2-src, r3-len
void w2cpy(void* buff_h, void* buff_l, void* src, uint32_t len)
{
__asm(

		"push {r4}\n"
		"loop:\n"
		"ldr r4, [r1]\n"   		// odczytujemy wartosc spod adresu r1
		"str r4, [r0], #4\n"	// zapis z lbuff do hbuff i inkrementaca hbuff
		"ldr r4, [r2], #4\n"	// odczyt wartosci z bufora zrodlowego
		"str r4, [r1], #4\n"	// zapis do lbuff i inkrementacja
		"subs r3,#1\n"
		"bne loop\n"
		"pop {r4}\n"
		);
}

// zwraca index najwiwiekszego elementu, oraz jego wartosc w pMaxVal
uint32_t f32absmax(float* pSrc, float* pMaxVal, uint32_t blockSize) {
	__asm(
			"push {r4}\n"
			"vldr.f32 s1,=0x0\n"
			"add r0, r0, r2, lsl #2\n"	// startujemy od konca czyli addrSrc + len*4 - 4
			"loop1:\n"
			"ldr r3, [r0, #-4]! \n"	// odczyt zmiennej
			"vmov s0, r3\n"			// przepisanie do rejestru float
			"vabs.f32 s0, s0\n"		// obliczenie abs
			"vcmp.f32 s0, s1\n"		// porownanie z poprzednia maks wartoscia
			"vmrs APSR_nzcv, fpscr\n"
			"itt gt\n"
			"vmovgt s1, s0\n"// 	// jezeli aktualna wartosc wieksza od poprzedniej maks to przepisanie
			"movgt r4, r2\n"
			"subs r2,#1\n"
			"bne loop1\n"
			"sub r3, r4, #1\n"
			"vstr.f32 s1, [r1]\n"
			"pop {r4}\n"
	);
}


void toggle_buff2(void* a, void* b)
{
__asm(

		"ldr r2, [r0]\n"  	//r2 = *a
		"ldr r3, [r1]\n"
		"str r2, [r1]\n"  	//b* = *a
		"str r3, [r0]\n" 	//a* = *b
		);
}

float test(float a)
{
	float b;
	b = a*a;

	return b;
}


/*
 __asm(
    		  "vsqrt.f32 %[result], %[value]\n"
    			:[result] "=t" (curr_rms_ma) : [value] "t" (rms_sqrt_sum)) ;


    	// je¿eli uzywa siê rejestrów inline innych ni¿ wejœciowe to trzeba o tym poinformowaæ kompilator "clobber"
    	//ARM-GCC-Inline-Assembler-Cookbook.pdf

 */

// reverses a string 'str' of length 'len'
void reverse(char *str, int len)
{
    int i=0, j=len-1, temp;
    while (i<j)
    {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++; j--;
    }
}

 // Converts a given integer x to string str[].  d is the number
 // of digits required in output. If d is more than the number
 // of digits in x, then 0s are added at the beginning.
int itoa(int x, char str[], int d)
{
    int i = 0;
    int xm = x;

    if(x < 0)
    	x = -x;

    while (x)
    {
        str[i++] = (x%10) + '0';
        x = x/10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    if(xm < 0)
    	str[i++] = '-';

    reverse(str, i);
    str[i] = '\0';
    return i;
}

void ftoa(float n, char *res, int afterpoint)
{
    // Extract integer part
    int ipart = (int)n;

    // Extract floating part
    float fpart = n - (float)ipart;

    if(fpart < 0)
    	fpart = -fpart;

    // convert integer part to string
    int i = itoa(ipart, res, 0);

    // check for display option after point
    if (afterpoint != 0)
    {
        res[i] = '.';  // add dot

        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter is needed
        // to handle cases like 233.007
        fpart = fpart * 10;

        itoa((int)fpart, res + i + 1, 1);
    }
}

uint32_t
swap(uint32_t in)
{
        return __builtin_bswap32(in);
}
