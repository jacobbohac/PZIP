#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "inc.h"
#include "arithc.h"
#include "intmath.h"
#include "det_escape.h"
#include "config.h"
#include "safe.h"

/*************************************************************************/
/*  This module is a dedicated sub-facility for deterministic.ch.        */
/*                                                                       */
/*  Our purpose here is to efficiently encode deterministic.c's escape   */
/*  symbols by making good estimates of their probabilities.             */
/*                                                                       */
/*  Our input for doing so are the                                       */
/*                                                                       */
/*       ulong key,                                                      */
/*       int   escape_count,                                             */
/*       int   total_symbol_count,                                       */
/*       int   followset_size                                           */
/*                                                                       */
/*  information which deterministic.c gives us each call:  We partition  */
/*                                                                       */
/*     key x escape_count x total_symbol_count x followset_size         */
/*                                                                       */
/*  space into bins, count the number of calls vs number of escapes      */
/*  for each bin over time, and make our predictions according to        */
/*  the escape/call ratio in the relevant bin.                           */
/*                                                                       */
/*  Actually, we partition our space three different ways (PARTITIONS),  */
/*  giving us three different predictions each call, which we then       */ 
/*  combine to produce our final prediction.                             */
/*                                                                       */
/*  By using both coarse partitions (which gather data more quickly      */
/*  and thus converge more rapidly to good predictions early in a        */
/*  file) and fine partitions (which take longer to converge to          */
/*  statistically significant predictions, but which give more           */
/*  nuanced predictions late in a file) we achieve reasonably            */
/*  balanced prediction performance throughout the input file.           */
/*                                                                       */
/*************************************************************************/


/*********************************************************************************************/

#define PARTITIONS 3

static int partition_bits[ PARTITIONS ] = { 7, 15, 16 };

#define partition_bins(x) (1 << partition_bits[ x ])

struct Escape {
    uword* esc[ PARTITIONS ];
    uword* tot[ PARTITIONS ];
};

static const int ZEsc_INIT_ESC   =  8;
static const int ZEsc_INIT_TOT   = 12;
static const int ZEsc_INIT_SCALE =  7;
static const int ZEsc_ESC_INC    = 17;
static const int ZEsc_ESCTOT_INC =  1;
static const int ZEsc_TOT_INC    = 17;


/*********************************************************************************************/

Escape* escape_Create( void ) {

    Escape* self = new( Escape );

    {   int  i;
        for (i = 0;   i < PARTITIONS;   i++) {
            self->esc[i] = safe_Malloc( partition_bins(i) * sizeof(uword) );
            self->tot[i] = safe_Malloc( partition_bins(i) * sizeof(uword) );
        }
    }

    /* Seed each partition's bins with our best a priori */
    /* guesses as to their escape probabilities:         */
    {   int      i, j;
        for     (i = 0;   i < PARTITIONS       ;   i++) {   /* For each partition            */
            for (j = 0;   j < partition_bins(i);   j++) {   /* For each bin in the partition */

                int esc = (j >> 0) & 0x03;  /* Recover the escape count for this bin. */
                int tot = (j >> 2) & 0x07;  /* Recover the call   count for this bin. */

                /* We scale our initial guesses just large enough */
                /* to have some accuracy, but small enough to be  */
                /* quickly overwhelmed by real data once they     */
                /* start arriving:                                */
                self->esc[i][j] = 1 + (ZEsc_INIT_SCALE * esc) + ZEsc_INIT_ESC;
                self->tot[i][j] = 2 + (ZEsc_INIT_SCALE * tot) + ZEsc_INIT_TOT + ZEsc_INIT_ESC;
            }
        }
    }

    return self;
}

void escape_Destroy(   Escape* self   ) {
    if (self) {
        int i;
        for (i = 0;   i < PARTITIONS;   ++i) {
            destroy( self->esc[i] );
            destroy( self->tot[i] );
        }

        free( self );
    }
}

/* An ephemeral return-value struct */
/* for the remaining functions:     */
typedef struct {
    int escape_count;
    int total_count;

    ulong bin[ PARTITIONS ];          /* Return values from pick_bins().          */
    bool  found_appropriate_bins;     /* TRUE iff bin[] currently contains stuff. */
} X;

static void update_escape_statistics(   Escape* self,   bool escape,   X x   ) {

    if (!x.found_appropriate_bins)   return;

    {   int  partition;
        for (partition = PARTITIONS;   partition --> 0;   ) {

            ulong  h    = x.bin[ partition ];

            uword* pEsc = &self->esc[ partition ][ h ];
            uword* pTot = &self->tot[ partition ][ h ];

            if (!escape) {
                *pTot += ZEsc_TOT_INC;                    /* 17     */
            } else {
                *pTot += ZEsc_ESC_INC + ZEsc_ESCTOT_INC;  /* 17 + 1 */
                *pEsc += ZEsc_ESC_INC;                    /* 17     */
            }

            if (*pTot > 16000) {
                *pTot >>= 1;
                *pEsc >>= 1;
                if (*pEsc < 1) {
                    *pEsc = 1;
                }
            }
        }
    }
}

static X pick_bins(   ulong key,   int escape_count,   int total_symbols_count,   int followset_size   ) {

    /***************************************************************/
    /* We partition input space into bins in three different ways. */
    /* For each way, figure out which bin we are in.               */
    /* Return the three results in x.bin[3].                       */
    /*                                                             */
    /* Actually, our bins don't cover all of the input space:      */
    /* We cluster them where we think they will do the most        */
    /* good.  If the input misses our net, we return with          */
    /*                                                             */
    /*   x.found_appropriate_bins == FALSE                         */
    /*                                                             */
    /* and our caller falls back to a default prediction method.   */
    /***************************************************************/

    X x;

    int total_count = escape_count + total_symbols_count;
        
    assert( escape_count >= 1 );
    assert( total_count >= 2 );

    if (followset_size > 3) {
        followset_size = 3;        /* Cap it at 2 bits. */
    }

    if (escape_count >= 4) {
        /* We didn't allocate any bins for this */
        /* situation, so just shrug and grin:   */
        x.found_appropriate_bins = FALSE;
        return x;
    }

    {   static ubyte total_code[] = {
            0,  /*  0 */
            1,  /*  1 */
            2,  /*  2 */
            3,  /*  3 */
            3,  /*  4 */
            4,  /*  5 */
            4,  /*  6 */
            5,  /*  7 */
            5,  /*  8 */
            5,  /*  9 */
            6,  /* 10 */
            6,  /* 11 */
            6,  /* 12 */
        };

        int counts = escape_count-1;
        int total = (total_count >= 15)   ?   7   :   total_code[ total_count -2 ];

        counts |= total << 2;   /* Lower 5 bits of 'counts' are now filled. */

        x.bin[2] = counts | (( ( key    &0x7F) + (((key>>13)&0x3)<<7)                                               + (followset_size<<9) )<<5);
        x.bin[1] = counts | (( ((key>>5)&0x03) + (((key>>13)&0x3)<<2) + (((key>>21)&0x3)<<4) + (((key>>29)&0x3)<<6) + (followset_size<<8) )<<5);
        x.bin[0] = counts |                                                                                           (followset_size      <<5);

        x.found_appropriate_bins = TRUE;
    }

    return x;
}

/* Define a little local synonym */
/* for 'intlog2r' to make the    */
/* code read better:             */
#undef  log2
#define log2 intlog2r

static X estimated_escape_probability(   Escape* self,   ulong key,   int escape_count,   int total_symbol_count,   int followset_size   ) {

    X x = pick_bins( key, escape_count, total_symbol_count, followset_size );

    if (!x.found_appropriate_bins) {
        /* Whoops -- no statistics for this case! *blush* */
        /* Fall back to a simple naive prediction:        */
        x.escape_count = escape_count;
        x.total_count = escape_count + total_symbol_count;
        return x;
    }

    /**************************************************************/
    /* Blend by entropy the predictions of our three different    */
    /* partition models.  This takes advantage of the information */
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


    {   /* First, extract the raw appearance counts from the three bins: */
        int e0 = self->esc[0][ x.bin[0] ];   int t0 = self->tot[0][ x.bin[0] ];
        int e1 = self->esc[1][ x.bin[1] ];   int t1 = self->tot[1][ x.bin[1] ];
        int e2 = self->esc[2][ x.bin[2] ];   int t2 = self->tot[2][ x.bin[2] ];

        /* Compute relative weights for the three predictions: */
        int w0 = (1 << 16) / (t0 * log2(t0) - e0 * log2(e0) - (t0-e0) * log2(t0-e0) + 1);
        int w1 = (1 << 16) / (t1 * log2(t1) - e1 * log2(e1) - (t1-e1) * log2(t1-e1) + 1);
        int w2 = (1 << 16) / (t2 * log2(t2) - e2 * log2(e2) - (t2-e2) * log2(t2-e2) + 1);

        /* Combine to produce our final prediction: */
        x.total_count  = w0*t0 + w1*t1 + w2*t2;
        x.escape_count = w0*e0 + w1*e1 + w2*e2;

        /* Scale our final answer to avoid overflow */
        /* in the arithmetic encoder:               */
        while (x.total_count >=  1 << 13+8) {  x.total_count  >>= 8;    x.escape_count >>= 8;        }
        if    (x.total_count >=  1 << 13+4) {  x.total_count  >>= 4;    x.escape_count >>= 4;        }
        if    (x.total_count >=  1 << 13+2) {  x.total_count  >>= 2;    x.escape_count >>= 2;        }
        if    (x.total_count >=  1 << 13+1) {  x.total_count  >>= 1;    x.escape_count >>= 1;        }

        /* If our prediction is insane -- change it! :) */
        if (x.escape_count < 1) {
            x.escape_count = 1;                  /* There is always -some- probability of getting an escape.     */
        }
        if (x.total_count <= x.escape_count) {
            x.total_count  = x.escape_count + 1; /* There is always -some- probability of NOT getting an escape. */
        }

        return x;                                /* Whew! :)                                                     */
    }
}
#undef  log2

void escape_Encode(   Escape* self,   Arith* arith,   ulong key,   int escape_count,   int total_symbol_count,   int followset_size,   bool escape   ) {

    X x  = estimated_escape_probability( self, key, escape_count, total_symbol_count, followset_size );   assert( escape_count >= 1 );

    update_escape_statistics( self, escape, x  );

    arith_Encode_Bit( arith, x.total_count - x.escape_count, x.total_count, escape );
}

bool escape_Decode(   Escape* self,   Arith* arith,   ulong key,   int escape_count,   int total_symbol_count,   int followset_size   ) {

    X x      = estimated_escape_probability( self, key, escape_count, total_symbol_count, followset_size );

    bool    escape = arith_Decode_Bit( arith, x.total_count - x.escape_count, x.total_count );           assert( escape_count >= 1 );

    update_escape_statistics( self, escape, x );

    return escape;
}



