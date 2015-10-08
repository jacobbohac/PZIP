#ifndef DETERMINISTIC_H
#define DETERMINISTIC_H

#include "inc.h"
#include "arithc.h"
#include "excluded_symbols.h"
#include "context.h"

typedef struct Det Det;

Det* deterministic_Create( void );

void deterministic_Destroy(   Det* self   );
void deterministic_Update(    Det* self,                     ubyte* input_ptr,                       int   symbol,                             Context* context );
bool deterministic_Encode(    Det* self,   Arith* arith,     ubyte* input_ptr,   ubyte* input_buf,   int   symbol,   Excluded_Symbols* excl,   Context* context );
bool deterministic_Decode(    Det* self,   Arith* arith,     ubyte* input_ptr,   ubyte* input_buf,   int* psymbol,   Excluded_Symbols* excl,   Context* context );

#endif /* DETETERMINISTIC_H */

