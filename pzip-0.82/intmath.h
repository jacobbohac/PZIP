#ifndef INTMATH_H
#define INTMATH_H

#include "inc.h"

extern void intmath_init( void );
extern uint ilog2round_tab[ 8192 ];
#define ilog2roundtab(i) ilog2round_tab[ (i) & 8191 ]


#ifndef ispow2
#define ispow2(x) (!( (x) & ~(-(x)) ))
#endif

extern int intlog2r(ulong N);    /* 'r' => rounded  */

extern uint ilog2x16(uint val);
extern int  flog2x16(float val); /* can be negative */

#ifdef DO_ASM_LOG2S //{

uint        ilog2ceil(uint val)
{
	__asm
	{
		FILD val
		FSTP val
		mov eax,val
		add eax,0x7FFFFF // 1<<23 - 1
		shr eax,23
		sub eax,127
	}
}

uint        ilog2floor(uint val)
{
	__asm
	{
		FILD val
		FSTP val
		mov eax,val
		shr eax,23
		sub eax,127
	}
}

uint        ilog2round(uint val)
{
	__asm
	{
		FILD val
		FSTP val
		mov eax,val
		add eax,0x257D86 // (2 - sqrt(2))*(1<<22)
		shr eax,23
		sub eax,127
	}
}

int        flog2(float xf) // !!! 
{
return ((*(int*)&xf) >> 23) - 127;
}

#else // }{

uint ilog2ceil(  uint val );
uint ilog2floor( uint val );
uint ilog2round( uint val );
int  flog2(      float f  );

#endif //}

#endif
 
