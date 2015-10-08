#ifndef ARITHC_H
#define ARITHC_H

#include "inc.h"

typedef struct Arith Arith;

extern Arith* arith_Create( void );
extern void   arith_Destroy(          Arith* arith );

extern void   arith_Start_Decoding(   Arith* arith,   ubyte* out_buf );
extern void   arith_Start_Encoding(   Arith* arith,   ubyte* out_buf ); /* DANGER! Writes to buf[-1] !! */
extern ubyte* arith_Finish_Encoding(  Arith* arith );


extern void   arith_Encode_Bit(       Arith* arith,   ulong p0,    ulong pt,   bool bit );
extern bool   arith_Decode_Bit(       Arith* arith,   ulong p0,    ulong pt );
extern void   arith_Encode_1_Of_N(    Arith* arith,   ulong low,   ulong high,   ulong total );
extern void   arith_Decode_1_Of_N(    Arith* arith,   ulong low,   ulong high,   ulong total );
extern ulong  arith_Get_1_Of_N(       Arith* arith,   ulong total );

#endif /* ARITHC_H */
