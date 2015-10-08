#ifndef ESCAPE_H
#define ESCAPE_H

#include "inc.h"

typedef struct Escape Escape;

Escape* escape_Create(  void         );
void    escape_Destroy( Escape* self );
void escape_Encode(     Escape* self,   Arith* arith,   ulong key,   int escC,   int totC,   int sym_count,   bool escape );
bool escape_Decode(     Escape* self,   Arith* arith,   ulong key,   int escP,   int totP,   int sym_count                );

#endif /* ESCAPE_H */
