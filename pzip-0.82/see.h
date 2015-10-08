#ifndef SEE_H
#define SEE_H

#include "inc.h"
#include "arithc.h"

#ifndef DEFINED_CONTEXT
typedef struct Context Context;
#define DEFINED_CONTEXT
#endif

typedef struct See_State See_State;
typedef struct See See;

See* see_Create(  void     );
void see_Destroy( See* see );

See_State* see_Get_State(     See* see,   uint escape_count,   uint tot_symbol_count,   ulong key,   const Context* context  );
void       see_Encode_Escape( See* see,   Arith* arith,   See_State* ss,   uint escape_count,   uint tot_symbol_count,   bool escape   );
bool       see_Decode_Escape( See* see,   Arith* arith,   See_State* ss,   uint escape_count,   uint tot_symbol_count );
void       see_Adjust_State(  See* see,   See_State* ss,   bool escape   );
uint       see_Estimate_Escape_Probability(  See* see,   See_State* ss,   uint escape_count,   uint tot_symbol_count   );

#endif /* SEE_H */

