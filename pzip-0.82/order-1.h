#ifndef ORDER_MINUS_ONE_H
#define ORDER_MINUS_ONE_H

#include "inc.h"
#include "excluded_symbols.h"
#include "arithc.h"

void order_minus_one_Encode( uint sym, uint num_chars, Arith* arith, Excluded_Symbols* excluded_symbols );
uint order_minus_one_Decode(           uint num_chars, Arith* arith, Excluded_Symbols* excluded_symbols );

#endif /* ORDER_MINUS_ONE_H */

