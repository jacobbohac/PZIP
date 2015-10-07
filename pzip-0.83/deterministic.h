#ifndef DETERMINISTIC_H
#define DETERMINISTIC_H

#include "inc.h"
#include "arithmetic-encoding.h"
#include "excluded_symbols.h"

typedef struct Det Det;

typedef struct Deterministic_Node       Deterministic_Node;
typedef struct Deterministic_Context    Deterministic_Context;

#include "context.h"

Det* deterministic_Create( void );

void deterministic_Destroy(   Det* self   );
void deterministic_Update(    Det* self,                     u08* input_ptr,                       int   symbol,                             Context* context );
bool deterministic_Encode(    Det* self,   Arith* arith,     u08* input_ptr,   u08* input_buf,   int   symbol,   Excluded_Symbols* excl,   Context* context );
bool deterministic_Decode(    Det* self,   Arith* arith,     u08* input_ptr,   u08* input_buf,   int* psymbol,   Excluded_Symbols* excl,   Context* context );

#endif /* DETETERMINISTIC_H */

