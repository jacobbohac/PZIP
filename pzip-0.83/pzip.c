#include "pzip.h"
#include "arithmetic-encoding.h"
#include <time.h>

#include <stdio.h>

#include "deterministic.h"
#include "context.h"
#include "excluded_symbols.h"
#include "order-1.h"
#include "config.h"

typedef struct {

    Arith*   arith;
    Excluded_Symbols* excluded_symbols;
    See*     see;
    Det*     det;
} Pzip;

static Pzip* pzip_create( void ) {

    Pzip*  pzip = new( Pzip );

    trie                   = trie_Create();

    pzip->arith            = arith_Create();
    pzip->excluded_symbols = excluded_symbols_Create();
    pzip->see              = see_Create();
    pzip->det          =     deterministic_Create();

    return pzip;
}

static void pzip_destroy( Pzip* pzip ) {

    excluded_symbols_Destroy( pzip->excluded_symbols );
    arith_Destroy(   pzip->arith   );
    see_Destroy(     pzip->see     );

    trie_Destroy( trie );

    /* Doing this is much faster, but ruins any encapsulation      */
    /* we might have had : eg. we destroy the context trie         */
    /* for ALL pzip objects:                                       */
    context_Destroy_All_Contexts();

    deterministic_Destroy( pzip->det );

    destroy( pzip );
}

/*********

 todo : weight between several LOE schemes based on performance

 eg. rating = LOE_success1 * rating1 + ...

**********/

static int choose_context(   Context* context[],   int contexts,   u32 key,   Excluded_Symbols* excl,   See* see   ) {

    /****************************************************/ 
    /* At any given point in the encoding (compression) */
    /* process, we have on hand several different order */
    /* Contexts, each offering a different prediction   */
    /* of the probability of seeing a given symbol next */
    /* in the input stream.                             */
    /*                                                  */
    /* Which one should we believe?                     */
    /*                                                  */
    /* That's our job in this routine.                  */
    /****************************************************/ 

    int best_i      = 0;
    int best_rating = 0;

    int  i;
    for (i = contexts;   i --> 0;   ) {

        Context*         c;
        Followset_Stats  stats;
        See_State*       ss = NULL;

        if (i == 0 && best_rating == 0)   return 0;   /* Only choice. */

        c = context[ i ];

        if (!c || c->total_symbol_count == 0)   continue;

        stats = context_Get_Followset_Stats_With_Given_Symbols_Excluded( c, excl );

        if (stats.total_count == 0)   continue;

        assert( stats.max_count >= 0 );


        /* Favor deterministic contexts: */
        if (c->followset_size > 1)   stats.total_count += stats.escape_count;
        /* Note that this makes us a use a different see_state for selection than we do for coding! */

        if (stats.total_count >= stats.escape_count) {
            ss = see_Get_State( see, stats.escape_count, stats.total_count, key, c );
        }

        {   int  rating = ((PZIP_INTPROB_ONE - see_Estimate_Escape_Probability( see, ss, stats.escape_count, stats.total_count ))
                          * stats.max_count ) / stats.total_count;
            if (rating > best_rating) {
                best_rating = rating;
                best_i      = i;
            }
        }
    }

    return best_i;

    /*********************************************************************/
    /* XXX Shouldn't we be finding a way to combine information from all */
    /*     available models, instead of using just one...???             */
    /*********************************************************************/
}

Contexts active_contexts;

uint pzip_Encode(   u08* input_buf,   uint input_len,   u08* encode_buf   ) {

    /* This is the top-level compression function.                             */
    /*   input_buf:  Contents of file to be compressed.                        */
    /*   encode_buf: Where to leave the result.                                */
    /*   return val: Compressed length -- count of valid bytes in encode_buf.  */

    int num_chose_loe[      PZIP_ORDER +1 ];
    int num_tried_by_order[ PZIP_ORDER +1 ];
    int num_coded_by_order[ PZIP_ORDER +1 ];
    int num_coded_det = 0;

    clock_t began_at = clock();

    Pzip*  pzip  = pzip_create();
    Arith* arith = pzip->arith;

    u08* input_ptr      =  input_buf;
    u08* input_buf_end  =  input_buf + input_len;

    assert( PZIP_SEED_BYTES > 0 );

    /* Seed a preamble: */
    memcpy( encode_buf, input_ptr, PZIP_SEED_BYTES );
    memset( input_ptr - PZIP_MAX_CONTEXT_LEN, PZIP_SEED_BYTE, PZIP_MAX_CONTEXT_LEN );
    input_ptr  += PZIP_SEED_BYTES;

    arith_Start_Encoding( arith, encode_buf + PZIP_SEED_BYTES );

    memset( num_chose_loe,      0, (PZIP_ORDER +1) * sizeof(int) );
    memset( num_tried_by_order, 0, (PZIP_ORDER +1) * sizeof(int) );
    memset( num_coded_by_order, 0, (PZIP_ORDER +1) * sizeof(int) );

    while (input_ptr < input_buf_end) {

        int symbol = *input_ptr;                      /* Current symbol to encode.             */
        u32 key  = getu32( input_ptr -4 );        /* Last four chars seen on input stream. */

        trie_Fill_Active_Contexts( input_ptr ); /* Must come before det_Enc(), cuz that uses the top Context node */

        excluded_symbols_Clear( pzip->excluded_symbols );

        if (deterministic_Encode(   pzip->det,   arith,   input_ptr,   input_buf,   symbol,   pzip->excluded_symbols,   active_contexts.c[ PZIP_ORDER ]   )) {

            ++ num_coded_det;

        } else {

            /* Try selected contexts until one encodes 'symbol': */
            int order = PZIP_ORDER+1;
            for(order = choose_context( active_contexts.c, order, key, pzip->excluded_symbols, pzip->see ),   ++ num_chose_loe[ order ];   ;
                order = choose_context( active_contexts.c, order, key, pzip->excluded_symbols, pzip->see )
            ){

                ++ num_tried_by_order[ order ];

                /* Try to code symbol using selected order model: */
                if (context_Encode( active_contexts.c[order], arith, pzip->excluded_symbols, pzip->see, key, symbol )) {
                    ++ num_coded_by_order[ order ];
                    break;
                }
                        
                if (order == 0) {
                    /* Encode raw with order -1: */
                    order_minus_one_Encode( symbol, 256, arith, pzip->excluded_symbols );
                    break;
                }
            }

            /* Did encode, now update the stats: */
            {   int coded_order = max( order, 0 );
                for (order = 0;   order <= PZIP_ORDER;   order++) {
                    context_Update( active_contexts.c[order], symbol, key, pzip->see, coded_order );
                }
            }
        }

        deterministic_Update( pzip->det, input_ptr, symbol, active_contexts.c[ PZIP_ORDER ] );

        ++ input_ptr;

        /* Maybe assure user we haven't crashed: */
        if (verbose   &&   (input_ptr - input_buf) % PZIP_PRINTF_INTERVAL == 0) {
            fprintf(stderr, "%d/%d\r", (input_ptr - input_buf), input_len );
            fflush( stderr );
        }
    }
    if (verbose) {
        clock_t clocks  = clock() - began_at;                        /* Do NOT combine   */
        double  secs    = (double)clocks / (double)CLOCKS_PER_SEC;   /* these two lines! */
        fprintf(stderr, "%d/%d\n", input_len, input_len );
        fprintf(stderr,"%s : %f secs = %2.1f %ss/sec\n", "encode", secs, (double)input_len / secs, "byte" );
    }



    {   uint encode_len = (arith_Finish_Encoding( arith ) - (encode_buf + PZIP_SEED_BYTES)) + PZIP_SEED_BYTES;

#ifdef OLD
        pzip_destroy( pzip );
#endif

        /* The arithc has stuffed 1 byte; put it back: */ 
        encode_buf[ PZIP_SEED_BYTES-1 ] = input_buf[ PZIP_SEED_BYTES-1 ];

        if (verbose) {
            printf( "o : %7s : %7s : %7s\n", "loe", "tried", "coded" );
            printf("d : %7d : %7d : %7d\n", input_len, input_len, num_coded_det );
            {   int  i;
                for (i = PZIP_ORDER+1;   i --> 0;   ) {
                    printf(
                        "%d : %7d : %7d : %7d\n",
                        i, num_chose_loe[i], num_tried_by_order[i], num_coded_by_order[i]
                    );
                }
            }
        }

        return encode_len;
    }
}

void pzip_Decode(   u08* output_buf,   uint output_len,   u08* encode_buf   ) {

    clock_t began_at = clock();
    Pzip*  pzip      = pzip_create();
    Arith* arith     = pzip->arith;

    u08* output_ptr     = output_buf;
    u08* output_buf_end = output_buf + output_len;

    memcpy( output_ptr, encode_buf, PZIP_SEED_BYTES );
    memset( output_ptr - PZIP_MAX_CONTEXT_LEN, PZIP_SEED_BYTE, PZIP_MAX_CONTEXT_LEN );

    output_ptr += PZIP_SEED_BYTES;
    encode_buf += PZIP_SEED_BYTES;

    arith_Start_Decoding( arith, encode_buf );

    while (output_ptr < output_buf_end) {

        int      symbol;
        u32    key      = getu32( output_ptr - 4 );;

        trie_Fill_Active_Contexts( output_ptr );

        excluded_symbols_Clear( pzip->excluded_symbols );

        if (!deterministic_Decode( pzip->det, arith, output_ptr, output_buf, &symbol, pzip->excluded_symbols, active_contexts.c[PZIP_ORDER] )) {

            /* Go down the orders: */
            int order = PZIP_ORDER+1;
            for(order = choose_context( active_contexts.c, order, key, pzip->excluded_symbols, pzip->see );   ;
                order = choose_context( active_contexts.c, order, key, pzip->excluded_symbols, pzip->see )
            ){

                /* Try to coder from order: */
                if (context_Decode( active_contexts.c[order], arith, pzip->excluded_symbols, pzip->see, key, &symbol )) {
                    break;
                }
                        
                if (order == 0) {
                    /* Decode raw with order -1: */
                    symbol = order_minus_one_Decode( 256, arith, pzip->excluded_symbols );
                    break;
                }
            }

            /* Did decode, now update the stats: */
            {   int coded_order = max( order, 0 );
                for (order = 0;   order <= PZIP_ORDER;   ++order) {
                    context_Update( active_contexts.c[order], symbol, key, pzip->see, coded_order );
                }
            }
        }

        deterministic_Update( pzip->det, output_ptr, symbol, active_contexts.c[ PZIP_ORDER ] );

        *output_ptr++ = symbol;
                
        /* Maybe assure user we haven't crashed: */
        if (verbose   &&   (output_ptr - output_buf) % PZIP_PRINTF_INTERVAL == 0) {
            fprintf(stderr, "%d/%d\r", (output_ptr - output_buf), output_len );
            fflush( stderr );
        }
    }

    if (verbose) {
        clock_t clocks  = clock() - began_at;                        /* Do NOT combine   */
        double  secs    = (double)clocks / (double)CLOCKS_PER_SEC;   /* these two lines! */
        fprintf(stderr, "%d/%d\n", output_len, output_len );
        fprintf(stderr,"%s : %f secs = %2.1f %ss/sec\n", "decode", secs, (double)output_len / secs, "byte" );
    }

    pzip_destroy( pzip );
}

