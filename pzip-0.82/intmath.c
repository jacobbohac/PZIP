#include "inc.h"
#include "intmath.h"

static unsigned char log2x16_table[256] = {     0,
     0,16,25,32,37,41,45,48,51,53,55,57,59,61,63,64,
    65,67,68,69,70,71,72,73,74,75,76,77,78,79,79,80,
    81,81,82,83,83,84,85,85,86,86,87,87,88,88,89,89,
    90,90,91,91,92,92,93,93,93,94,94,95,95,95,96,96,
    96,97,97,97,98,98,98,99,99,99,100,100,100,101,101,101,
    101,102,102,102,103,103,103,103,104,104,104,104,105,105,105,105,
    106,106,106,106,107,107,107,107,107,108,108,108,108,109,109,109,
    109,109,110,110,110,110,110,111,111,111,111,111,111,112,112,112,
    112,112,113,113,113,113,113,113,114,114,114,114,114,114,115,115,
    115,115,115,115,116,116,116,116,116,116,116,117,117,117,117,117,
    117,117,118,118,118,118,118,118,118,119,119,119,119,119,119,119,
    119,120,120,120,120,120,120,120,121,121,121,121,121,121,121,121,
    121,122,122,122,122,122,122,122,122,123,123,123,123,123,123,123,
    123,123,124,124,124,124,124,124,124,124,124,125,125,125,125,125,
    125,125,125,125,125,126,126,126,126,126,126,126,126,126,126,127,
    127,127,127,127,127,127,127,127,127,127,128,128,128,128,128
};

#ifndef DO_ASM_LOG2S //{

uint ilog2round( uint val ) {
#ifdef _MSC_VER
        __asm
        {
                FILD val
                FSTP val
                mov eax,val
                add eax,0x257D86 // (2 - sqrt(2))*(1<<22)
                shr eax,23
                sub eax,127
        }
#else

    uint u;
    for (u = 1;   (1ul << u) <= val;   ++u) ;
    -- u;
    assert( val >= (1UL << u) );
    val <<= (16 - u);
    assert(   val >= 65536   &&   val < 65536*2   );
    if (val >= 92682) { // sqrt(2) * 1<<16
        ++u;
    }
    return u;

#endif
}

uint ilog2round_tab[ 8192 ];

void intmath_init( void ) { int i;  for (i = 8192;  i --> 1; )  ilog2round_tab[i] = ilog2round( i );  }


uint ilog2ceil( uint val ) {
#ifdef _MSC_VER

        __asm
        {
                FILD val
                FSTP val
                mov eax,val
                add eax,0x7FFFFF // 1<<23 - 1
                shr eax,23
                sub eax,127
        }

#else

    uint u;
    for (u = 0;   (1ul << u) < val;   u++) ;
        // (1<<u) >= val;

    return u;

#endif
}

uint ilog2floor( uint val ) {
#ifdef _MSC_VER

        __asm
        {
                FILD val
                FSTP val
                mov eax,val
                shr eax,23
                sub eax,127
        }

#else

    uint u;
    for (u = 1;   (1ul << u) <= val;   u++) ;
        // (1<<l) > val;

    return u - 1;

#endif
}

int flog2( float f ) {
    return ((*(int*)&f) >> 23) - 127;
}

#endif //}

int flog2x16( float val ) {
    int il, frac, lx16;

    if (val <= 0.0f)   return 0;

    if (val < 1.0f) {
        il = 0;
        while (val < 1.0f) {
            --il;
            val *= 2.0f;
        }
                
        frac = (uint)(val * 128.0f);

    } else {

        il = flog2( val );

        assert( val >= (1UL << il) );

        frac = (uint)(val * 128.0f);
        frac = frac >> il;
                
        // val = 2^(il) * (frac/128)
    }

    assert( frac >= 128 && frac < 256 );

    lx16 = il << 4;

    il = log2x16_table[ frac ] - 112;
    assert( il >= 0 && il <= 16 );

    lx16 += il;

    return lx16;
}

uint ilog2x16( uint val ) {
    
    uint il, frac, lx16;

    if (val <= 1)   return 0;

    il = ilog2floor( val );

    assert( val >= (1UL << il) );

    frac = (val << 7) >> il;

    // val = 2^(il) * (frac/128)

    assert( frac >= 128 && frac < 256 );

    lx16 = il << 4;

    il = log2x16_table[ frac ] - 112;
    assert( il >= 0 && il <= 16 );

    lx16 += il;

    return lx16;
}

int intlog2r( ulong n ) { /** rounded **/

     static unsigned char rbits[256] = {
         0,0,1,2,2,2,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,
         5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
         6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
         6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
         8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
         8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
         8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
         8,8,8,8,8
     };

     if (n >> 16) { if (n >> 24)   return 24 + rbits[ n >> 24 ];
                    else           return 16 + rbits[ n >> 16 ];
     } else {       if (n >> 8)    return  8 + rbits[ n >>  8 ];
                    else           return  0 + rbits[ n >>  0 ];
     }
}

