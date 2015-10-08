/********************************************

 Secondary Escape Estimation ("see")

 This version does 3-order weighting by entropy

********************************************/

#include <stdlib.h>

#include "see.h"
#include "config.h"
#include "context.h"

#include "intmath.h"

// The hash is:
// top  5 bits are esc/tot
// next 2 are order
// then 2 for # of chars in parent cntx
// then four pairs of 2 from cntx
// then 5 bits from order0

#define ORDER0_BITS ( 9)        /* <> These cutoffs and the hashes could all be tuned. */
#define ORDER1_BITS (16)
#define ORDER2_BITS (23)

#define ORDER0_SIZE (1 << ORDER0_BITS)
#define ORDER1_SIZE (1 << ORDER1_BITS)
#define ORDER2_SIZE (1 << ORDER2_BITS)

#define MAX_SEE_ESCC    ( 3)
#define MAX_SEE_TOTC    (64)

struct See_State {
    See_State* parent;
    uint       seen;
    uint       escapes;
    uint       total;
};

struct See {
    See_State order0[ ORDER0_SIZE ];
    See_State order1[ ORDER1_SIZE ];
    See_State order2[ ORDER2_SIZE ];
};

static uint tottab[] = {
    0,    /* 0 */ 
    1,    /* 1 */ 
    2,    /* 2 */ 
    3,    /* 3 */ 
    5,    /* 4 */ 
    8,    /* 5 */ 
   11,    /* 6 */ 
   20,    /* 7 */ 
};

static See* initialize( See* see ) {

    uint e;
    for (e = 0;   e <= 3;   ++e) {

        uint escape_count = e + 1;

        uint t;
        for (t = 0;   t <= 7;   ++t) {

            uint total_count        = tottab[ t ];
            uint total_symbol_count = total_count + escape_count;

            /* The five bit esc/tot: */
            uint h_hi        = (e << 3) + t;
            uint seed_escape = escape_count * SEE_INIT_SCALE + SEE_INIT_ESC;
            uint seed_total  = (escape_count + total_symbol_count) * SEE_INIT_SCALE + SEE_INIT_TOT;
            uint shift       = ORDER1_BITS - 5;

            uint h_lo;
            for (h_lo = 0;   h_lo < ((uint)1 << shift);   ++h_lo) {

                uint hash = (h_hi << shift) | h_lo;                    assert( hash < ORDER1_SIZE );

                {   See_State* ss = &see->order1[ hash ];

                    ss->escapes  = seed_escape;
                    ss->total    = seed_total;
                    ss->parent   = &see->order0[ hash >> (ORDER1_BITS - ORDER0_BITS) ];

                    ss           = ss->parent;
                    ss->escapes  = seed_escape;
                    ss->total    = seed_total;
                }
            }
        }
    }

    return see;
}

See* see_Create( void )      {   return initialize( calloc( 1, sizeof( See ) ) );   }
void see_Destroy( See* see ) {   destroy( see );                                    }

/* Define a local synonym for readability: */
#undef  log2
#define log2 ilog2roundtab


/* Local ephemeral return type: */
typedef struct {
    uint escapes;
    uint total;
} X;

static X get_stats(   See* see,   See_State* ss2,   uint inEsc,   uint inTot   ) {

    See_State* ss1 = ss2->parent;
    See_State* ss0 = ss1->parent;

    uint e0 = ss0->escapes;   uint t0 = ss0->total;   uint s0 = ss0->seen;
    uint e1 = ss1->escapes;   uint t1 = ss1->total;   uint s1 = ss1->seen;   
    uint e2 = ss2->escapes;   uint t2 = ss2->total;   uint s2 = ss2->seen;

    uint w0 = (1 << 16) / (t0 * log2(t0) - e0 * log2(e0) - (t0-e0) * log2(t0-e0) + 1);
    uint w1 = (1 << 16) / (t1 * log2(t1) - e1 * log2(e1) - (t1-e1) * log2(t1-e1) + 1);
    uint w2 = (1 << 16) / (t2 * log2(t2) - e2 * log2(e2) - (t2-e2) * log2(t2-e2) + 1);

    /* Give less weight to contexts with only the default stats. */
    /* This helps a bit; *2,3, or 4 seems the best multiple:     */
    if (s0)   w0 <<= 2;
    if (s1)   w1 <<= 2;
    if (s2)   w2 <<= 2;

    {   X x;
        {   /* Also weight in the esc/tot from the context. */
            /* This helps about 0.001 bpc on most files.    */

            uint ei = inEsc;
            uint ti = inTot;
            uint wi = (1 << 16) / (ti * log2(ti) - ei * log2(ei) - (ti-ei) * log2(ti-ei) + 1);

            /**************************************************************/
            /* Blend by entropy the predictions of our different models.  */
            /* This takes advantage of the information                    */
            /* in all of them (versus just picking "the best"), but       */
            /* weights them according to the amount of evidence backing   */
            /* them, in intuitive terms.                                  */
            /**************************************************************/

            /**************************************************************/
            /* 2004-05-10-CrT: I asked Charles Bloom to comment on the    */
            /* reasoning behind this code:                                */
            /*                                                            */
            /*  "The entropy-based weighting is a heuristic that favors   */
            /*   contexts we are confident in.  You start with SEE that   */
            /*   predicts a  50/50 escape or no-escape, which is a high   */
            /*   entropy (1 bit).  The SEE contexts that become more      */
            /*   deterministic (eg. highly predictable) have lower        */
            /*   entropies ; we want to use those ones, so we divide      */
            /*   by the entropy."                                         */
            /*                                                            */
            /**************************************************************/

            x.total   = w0*t0 + w1*t1 + w2*t2 + wi*ti;
            x.escapes = w0*e0 + w1*e1 + w2*e2 + wi*ei;
        }

        /* Renormalize to avoid overflow in the arithmetic encoder: */
        while (x.total >= 16000) {
            x.total   >>= 1;
            x.escapes >>= 1;
        }

        /* If our prediction is insane -- change it! :) */
        if (x.escapes < 1) {
            x.escapes = 1;                  /* There is always -some- probability of getting an escape.     */
        }
        if (x.total <= x.escapes) {
            x.total  = x.escapes + 1;       /* There is always -some- probability of NOT getting an escape. */
        }

        return x;
    }
}

#undef  log2


void  see_Encode_Escape(   See* see,   Arith* arith,   See_State* ss,   uint escape_count,   uint total_symbol_count,   bool escape   ) {
    if (!ss) {
        arith_Encode_Bit(   arith,   total_symbol_count,   escape_count + total_symbol_count,   escape   );
    } else {
        X x = get_stats(   see,   ss,   escape_count,   escape_count + total_symbol_count   );
        arith_Encode_Bit( arith, x.escapes, x.total, !escape );
        see_Adjust_State( see, ss, escape );
    }
}

bool see_Decode_Escape(   See* see,   Arith* arith,   See_State* ss,   uint escape_count,   uint total_symbol_count   ) {
    if (!ss) {
        return arith_Decode_Bit(   arith,   total_symbol_count,   escape_count + total_symbol_count   );
    } else {
        X    x      = get_stats(   see,   ss,   escape_count,   escape_count + total_symbol_count   );
        bool escape = arith_Decode_Bit( arith, x.escapes, x.total );
        see_Adjust_State( see, ss, !escape );
        return !escape;
    }
}

uint see_Estimate_Escape_Probability(   See* see,   See_State* ss,   uint escape_count,   uint total_symbol_count ) {
    if (ss) {
        X x = get_stats(   see,   ss,   escape_count,   escape_count + total_symbol_count   );
        return   (x.escapes << PZIP_INTPROB_SHIFT) / x.total;
    }
    return   (escape_count << PZIP_INTPROB_SHIFT) / (escape_count + total_symbol_count);
}

void see_Adjust_State(   See* see,   See_State* ss,   bool escape   ) {

    for (;   ss;   ss = ss->parent) {
        
        ++ ss->seen;

        if (escape) {

            ss->escapes += SEE_INC;
            ss->total   += SEE_INC + SEE_ESC_TOT_EXTRA_INC;

        } else {

            /* Forget escapes quickly: */
            if (ss->escapes >= SEE_ESC_SCALE_DOWN) {
                ss->escapes  = (ss->escapes >> 1) + 1;
                ss->total    = (ss->total   >> 1) + 2;
            }
            ss->total += SEE_INC;
        }

        if (ss->total >= SEE_SCALE_DOWN) {
            ss->escapes = (ss->escapes >> 1) + 1;
            ss->total = (ss->total >> 1) + 2;
            assert( ss->total < SEE_SCALE_DOWN );
        }
    }
}


static void stats_from_hash(   See_State* ss,   uint five_bits   ) {

    uint e = five_bits >> 3;
    uint t = five_bits  & 7;

    uint total        = tottab[ t ];
    uint escape_count = e + 1;

    uint total_symbol_count = total + escape_count;

    uint seed_escape = escape_count * SEE_INIT_SCALE + SEE_INIT_ESC;
    uint seed_total  = (escape_count + total_symbol_count) * SEE_INIT_SCALE + SEE_INIT_TOT;
        
    ss->escapes = seed_escape;
    ss->total   = seed_total;
}

See_State* see_Get_State(   See* see,   uint escape_count,   uint total_symbol_count,   ulong key,   const Context* context   ) {

    // Do the hash;
    //      order
    //      escapes,
    //      total,
    //      key
    // use MPS count ?

    int order        = context->order;
    int symbol_count = context->followset_size;

    uint escapes = escape_count;
    uint total   = total_symbol_count;

    if (total == 0)   return NULL;

    assert( symbol_count >= 1       );
    assert( escapes      >= 1       );
    assert( total        >= escapes );

    total -= escapes;
    -- escapes;

    if (escapes >  MAX_SEE_ESCC
    || total    >= MAX_SEE_TOTC
    ){
        return NULL;
    }
    
    /*******************************************************************/
    /*  <> Tune the see hash!                                          */
    /*                                                                 */
    /*  We could make a much fancier hash;  Right now there are lots   */
    /*  of redundant bits;                                             */
    /*                                                                 */
    /*  eg. When order = 0 we dont use any bits for key, so we could   */
    /*      use more for esc & tot  conversely when esc is large,      */
    /*      we should use fewer bits for order & key                   */
    /*                                                                 */
    /*  This all makes it much messier to do the initialization...     */
    /*                                                     --cb        */
    /*******************************************************************/

    {   /****************************/
        /* Fill up 15 bits of hash: */
        /****************************/

        static uint tottab [] = {
            0,      /*  0 */
            1,      /*  1 */
            2,      /*  2 */
            3,      /*  3 */
            3,      /*  4 */
            4,      /*  5 */
            4,      /*  6 */
            5,      /*  7 */
            5,      /*  8 */
            5,      /*  9 */
            6,      /* 10 */
            6,      /* 11 */
            6,      /* 12 */
            6,      /* 13 */
        };

        /* Two bits for the escapes <= 3 */
        uint hash2 = escapes << 3;              assert( escapes <= 3 );

        /* Three bits for total: */
        hash2 |=   (total <= 13)   ?   tottab[ total ]   :   7;

        /* Two bits for the order: */
        hash2 <<= 2;
        hash2 |=   (escapes >= 1)   ?   (order >= 3)   :   min( order >> 1, 3 );

        /* Two bits for num chars in parent                                        */
        /* This was Malcolm's idea, and it helps *huge* (meaning about 0.02 bpc)   */
        /*      paper2 -> 2.196 and trans -> 1.229 !!!                             */
        /* Maybe I should use the actual-coded-parent by LOE instead of the direct */
        /* parent?  There is a problem there : the LOE decision depends on this!   */
        hash2 <<= 2;
        if (context->parent) {
            hash2 |= min( context->parent->followset_size, 3 );
        }

        /* isdet bool ?                                                       */
        /* Helps a tiny bit (0.001) on files with lots of dets (trans, bib).  */
        /* Doesn't affect others.                                             */
        hash2 <<= 1;
        hash2 |= (symbol_count == 1);

        /* Eight bits from key;  two bits from each of the last four bytes: */
        if (order > 0) { hash2 <<= 2; hash2 |= ((key >>  5) & 0x3); }
        if (order > 1) { hash2 <<= 2; hash2 |= ((key >> 13) & 0x3); }
        if (escapes <= 1) {
            if (order > 2) { hash2 <<= 2;  hash2 |= ((key >> 21) & 0x3); }
            if (order > 3) { hash2 <<= 2;  hash2 |= ((key >> 29) & 0x3); }
        }

        /* The bottom 5 bits of key[0]: */
        hash2 <<= 5;
        hash2 |= key & 31;        assert( hash2 < ORDER2_SIZE );

        {   uint hash1 = hash2 >> (ORDER2_BITS - ORDER1_BITS);
            See_State* ss1 = &see->order1[ hash1 ];
            See_State* ss2 = &see->order2[ hash2 ];

            if (!ss2->parent) {

                ss2->parent = ss1;
                stats_from_hash( ss2, hash2 >> (ORDER2_BITS - 5) );
            }
            return ss2;
        }
    }
}


