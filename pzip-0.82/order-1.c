#include "order-1.h"

/*******

 Order (-1) coder

 Totally flat probabilities with exclusions

*********/

void order_minus_one_Encode(   uint symbol,   uint char_count,   Arith* arith,   Excluded_Symbols* excl   ) {

    assert( ! excluded_symbols_Contains( excl, symbol ) );

    {   uint low = 0;
        uint i;
        for (i = 0;   i < symbol;   i++) {
            if (!excluded_symbols_Contains( excl, i ))  ++low;
        }
        {   uint total = low +1;
            for (i = symbol +1;   i < char_count;   i++) {
                if (!excluded_symbols_Contains( excl, i ))   ++total;
            }

            arith_Encode_1_Of_N( arith, low, low +1, total );
        }
    }
}

uint order_minus_one_Decode(   uint char_count,   Arith* arith,   Excluded_Symbols* excl   ) {

    uint total = 0;
    {   uint i;
        for (i = 0;   i < char_count;   i++) {
            if (!excluded_symbols_Contains( excl, i ))   ++total;
        }
    }

    {   uint target = arith_Get_1_Of_N( arith, total );

        arith_Decode_1_Of_N( arith, target, target +1, total );

        {   uint symbol;
            for (symbol = 0;   ;   ++symbol, --target) {
                while (excluded_symbols_Contains( excl, symbol ))   ++symbol;
                if (!target)   return symbol;
            }
        }
    }
}
