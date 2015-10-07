#ifndef ARITHMETIC_ENCODING_H
#define ARITHMETIC_ENCODING_H

#include "inc.h"

typedef struct Arith Arith;

extern Arith* arith_Create( void );
extern void   arith_Destroy(          Arith* arith );

extern void   arith_Start_Decoding(   Arith* arith,   u08* out_buf );
extern void   arith_Start_Encoding(   Arith* arith,   u08* out_buf ); /* DANGER! Writes to buf[-1] !! */
extern u08* arith_Finish_Encoding(  Arith* arith );


extern void   arith_Encode_Bit(       Arith* arith,   u32 p0,    u32 pt,   bool bit );
extern bool   arith_Decode_Bit(       Arith* arith,   u32 p0,    u32 pt );
extern void   arith_Encode_1_Of_N(    Arith* arith,   u32 low,   u32 high,   u32 total );
extern void   arith_Decode_1_Of_N(    Arith* arith,   u32 low,   u32 high,   u32 total );
extern u32  arith_Get_1_Of_N(       Arith* arith,   u32 total );

#endif /* ARITHMETIC_ENCODING_H */
