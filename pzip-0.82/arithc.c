#include "inc.h"
#include "arithc.h"
#include "safe.h"

/******************************************************/
/* Conventional computer media are organized by bits, */
/* -- the smallest unit of information one can write  */
/* is one bit -- but in file compression one often    */
/* needs to store some fraction of a bit.  Using a    */
/* full bit to do so would waste space, which is      */
/* counter-productive in a file compression program.  */
/*                                                    */ 
/* "Arithmetic encoding" is the standandard technique */
/* for solving this problem.  For the details of how  */
/* and why it works, feed "arithmetic encoding" into  */
/* your favorite search engine.                       */
/*                                                    */ 
/* The core API is:                                   */
/*                                                    */ 
/* o  To encode information, do                       */
/*                                                    */ 
/*        arith_Encode_Bit( arith, mid, tot, bit );   */
/*                                                    */ 
/*    where 'bit' is:                                 */
/*      0 with probability        mid  / tot          */ 
/*      1 with probability   (tot-mid) / tot          */ 
/*                                                    */ 
/* o  To decode information, do the converse:         */
/*                                                    */ 
/*        bit = arith_Encode_Bit( arith, mid, tot );  */
/*                                                    */ 
/******************************************************/


/* "Consts from Michael Schindler" -- cb */

/* We assume a 32-bit word with the high bit reserved */
/* for carry detection, leaving 31 bits for data:     */
#define BASE_BITS  31

/* How many bits do we need to shift to leave the top */
/* 8 data bits in the low byte of the word?           */
#define SHIFT_BITS (BASE_BITS - 8)

/* We fit three complete base bytes in a 32-bit word: */
#define BASE_BYTES ((BASE_BITS+7)/8)

/* Coding is done to this accuracy,   */
/* in terms of range>>PRECISION_BITS: */
#define PRECISION_BITS  9               

/* We mostly think of our base word as a 32-bit       */
/* fixed-point word with the binary point to the      */
/* right of the top bit.  Thus, in this notation 1.0  */
/* is represented by a 1 in the high bit and 0        */
/* everywhere else.  In practice, we will be working  */
/* only with values 0.0 <= p < 1.0, so any time the   */
/* top bit is set, that is a carry-out:               */
#define ONE          ((ulong)1 << BASE_BITS)


/* As with most any floating-point type implementation,  */
/* we want to keep our numbers from getting too          */
/* denormalized -- we want the leading '1' in the number */
/* to be reasonably far to the left, because the farther */
/* it is to the right, the less precision (accuracy) we  */
/* have.                                                 */
/*                                                       */
/* A good practical rule is to keep the first 1 within   */
/* the top byte.                                         */
/*                                                       */
/*  We do this when encoding by every now and then       */
/* shifting a byte to output.                            */
/*                                                       */
/* We do this when decoding by every now and then        */
/* shifting a byte in from the input buffer.             */
#define MIN_WIDE    ((ulong)1 << SHIFT_BITS)


/*********************************************************/
/*                 The Output Queue                      */
/*                                                       */
/* There is a problem on output in that we don't want to */
/* write out a byte until we are sure it will not be     */
/* changed in the future -- since unwriting and then     */
/* rewriting output is messy.                            */
/*                                                       */
/* The means that we may have to buffer up internally    */
/* an unbounded amount of pending output, because there  */
/* is no a priori limit on how far a carry may propagate.*/
/*                                                       */
/* However, a carry can only progage through a byte if   */
/* that byte is 0xFF.  Thus, of the unbounded amount of  */
/* output which we must be able to buffer, all but the   */
/* leading (most significant) byte must be 0xFF.  This   */
/* means that all we need to implement our 'unbounded'   */
/* output queue is in fact one variable to hold the      */
/* leading byte, plus a count of the number of following */
/* 0xFF bytes.  Thus our fields:                         */
/*                                                       */  
/*     ulong queued_ff_bytes;                            */
/*     ulong queued_byte;                                */
/*                                                       */  
/*********************************************************/


/* base and width of remaining free space. */
/* (See Arithmetic-Encoding.doc.)          */ 
typedef struct {
    ulong base;
    ulong wide;
} Free;

struct Arith {
    Free   free;
    ubyte* out_ptr;           /* Where to write next output byte. */
    ulong  queued_byte;
    ulong  queued_ff_bytes;   /* Used by encoder only.: */
};


/* When doing 1_of_N encoding/decoding, we need to set */
/* a limit on the sum of the given probabilities, else */
/* we risk running out of precision:                   */
#define CUMULATIVE_PROBABILITY_MAX (MIN_WIDE >> PRECISION_BITS)

#define BASE_MASK    (ONE - 1)

/* EXTRA_BITS are the seven code bits in a 32-bit */
/* word  which aren't included in our BASE_BYTES: */
/* that don't quite fit in bytes: (cb) */
#define EXTRA_BITS      ((BASE_BITS-1) % 8 + 1)

#define TAIL_EXTRA_BITS	(8 - EXTRA_BITS)     /* == 1 */

Arith* arith_Create( void          ) {   return new( Arith );      }
void   arith_Destroy( Arith* arith ) {   if (arith) free(arith);   }

static void flush_output_queue(   Arith* arith,   ubyte carry   ) {

    /* Send the queued non-0xFF byte, first adding any carry to it: */
    *arith->out_ptr++ = arith->queued_byte + carry;

    /* Now send the queued 0xFF bytes, if any,      */
    /* possibly flipped to 0x00 bytes by the carry: */
    for (;   arith->queued_ff_bytes;   --arith->queued_ff_bytes) {
        *arith->out_ptr++ = 0xFF + carry;
    }
}

static void renormalize_and_write_if_needed(   Arith* arith   ) {

    Free f = arith->free;    assert( f.wide <= ONE );

    while (f.wide <= MIN_WIDE) {

        ulong byte = f.base >> SHIFT_BITS;
        
        if (byte == 0xFF) {

            ++ arith->queued_ff_bytes;

        } else {

            /* Carry is in base's most significant bit: */
            flush_output_queue( arith, f.base >> BASE_BITS );

            /* Queue the new output byte: */
            arith->queued_byte = byte;
        }
        f.base = (f.base << 8) & BASE_MASK;
        f.wide = (f.wide << 8);
    }

    assert( f.wide <= ONE );

    arith->free = f;
}

static Free renormalize_and_read_if_needed(   Arith* arith   ) {

    Free f = arith->free;                 assert( f.wide <= ONE );

    while (f.wide <= MIN_WIDE) {
        f.wide <<= 8;
        f.base = (f.base << 8) + (((arith->queued_byte) << EXTRA_BITS) & 0xFF);   /* Use the top bit in the queue */
        f.base += (arith->queued_byte = *arith->out_ptr++) >> (TAIL_EXTRA_BITS);
    }
    assert( f.wide <= ONE );

    return f;
}



void arith_Start_Decoding(   Arith* arith,   ubyte* out_buf   ) {

    arith->out_ptr = out_buf;

    /**  'base' needs to be kept filled with 31 bits ;
     *       This means we cannot just read in 4 bytes.  We must read in 3,
     *       then the 7 bits of another (EXTRA_BITS == 7) , and save that last
     *       bit in the queue 
     **/

    arith->free.base = (arith->queued_byte = *arith->out_ptr++) >> TAIL_EXTRA_BITS;
    arith->free.wide = 1 << EXTRA_BITS;
}


void arith_Start_Encoding(   Arith* arith,   ubyte* out_buf   ) {

    arith->out_ptr = out_buf-1;

    arith->free.base = 0;
    arith->free.wide = ONE;

    arith->queued_byte     = 0;   /* This is a waste of a byte. */
    arith->queued_ff_bytes = 0;
}

ubyte* arith_Finish_Encoding( Arith* arith ) {

    uint wide_mask;
    uint wide_msb;

    /* Set 'base' to the maximum that won't change how it decodes: */
    arith->free.base += arith->free.wide - 1;

    flush_output_queue(   arith,   (arith->free.base & ONE) != 0   );

    /*****

    The minimal way to flush is to do :

                free.base += (free.wide - 1);

                clear 'base' below MSB of 'wide'

    eg. if free.wide is 67 we do :
                
                free.base += 66;
                free.wide &= ~63;

        then we just send 'base' bytes until the remainder is zero.

        (this assumes that when the decoder reads past EOF, it reads zeros!)

        -----

        we almost always write just 1 byte
        (in fact, I think we might *always* write 1 byte)

        ******/

    if (arith->free.wide >= ((ulong)1 << 31)) {
        wide_msb = (ulong)1 << 31;
    } else {
        for (wide_msb = 1;   wide_msb <= arith->free.wide;   wide_msb <<= 1) ;
        wide_msb >>= 1;
        assert( arith->free.wide >= wide_msb
             && arith->free.wide <  wide_msb * 2
        );
    }

    /* Clear 'base' under wide_mask: */
    wide_mask  = 0;
    wide_msb >>= 1;
    while (wide_msb) {
        wide_mask |= wide_msb;
        wide_msb >>= 1;
    }

    assert( wide_mask < arith->free.wide );

    arith->free.base &= ~wide_mask;
    arith->free.base &=  BASE_MASK;

    while (arith->free.base) {
        *arith->out_ptr++ = (arith->free.base >> SHIFT_BITS) & 0xFF;
        arith->free.base <<= 8;
        arith->free.base  &= BASE_MASK;
    }

    arith->out_ptr[0] = 0;
    arith->out_ptr[1] = 0;
    arith->out_ptr[2] = 0;
    arith->out_ptr[3] = 0;
    arith->out_ptr[4] = 0;
    arith->out_ptr[5] = 0;

    return arith->out_ptr;
}

ulong arith_Get_1_Of_N(   Arith* arith,   ulong total   ) {
    /* Read Arithmetic-Encoding.doc if you find this function mysterious! */

    Free f = renormalize_and_read_if_needed( arith );

    ulong ratio = f.wide / total;

    ulong ret = f.base / ratio;      assert( total <= CUMULATIVE_PROBABILITY_MAX );

    arith->free = f;

    return   ret >= total ? total-1 : ret;
}

void arith_Decode_1_Of_N(   Arith* arith,   ulong low,   ulong high,   ulong total   ) {
    /* Read Arithmetic-Encoding.doc if you find this function mysterious! */


    ulong ratio          = arith->free.wide / total;
    ulong base_decrement = ratio * low;
    arith->free.base    -= base_decrement;

    assert( low < high   &&   high <= total );
    assert( total <= CUMULATIVE_PROBABILITY_MAX );

    if (high == total)   arith->free.wide -= base_decrement;
    else                 arith->free.wide  = ratio * (high - low);
}

void arith_Encode_Bit(   Arith* arith,   ulong mid,   ulong total,  bool bit   ) {
    /* Read Arithmetic-Encoding.doc if you find this function mysterious! */

    Free f = arith->free;

    ulong r = (f.wide / total) * mid;

    if (bit) {   f.base += r;   f.wide -= r;    }
    else     {                  f.wide  = r;    }

    arith->free = f;

    renormalize_and_write_if_needed( arith );
}

void arith_Encode_1_Of_N(   Arith* arith,   ulong low,   ulong high,   ulong total   ) {
    /* Read Arithmetic-Encoding.doc if you find this function mysterious! */

    /* On a scale where the free zone is currently 'total' units tall, */
    /* arithmetically encode a symbol whose partition run from         */
    /* 'low' to 'high':                                                */


    /*******************************************************/
    /* We want to do                                       */ 
    /*                                                     */
    /*       base_increment = (wide * low) / total         */
    /*                                                     */
    /*  but wide & total can both be large, potentially    */
    /*  overflowing  the register, so we instead do:       */
    /*******************************************************/

    Free f = arith->free;

    ulong ratio          = f.wide / total;
    ulong base_increment = ratio * low;

    assert( low < high   &&   high <= total );
    assert( total <= CUMULATIVE_PROBABILITY_MAX );

    f.base += base_increment;

    /* Special-case the top partition to avoid  */
    /* problems due to ratio integer round-off: */
    if (high == total)   f.wide -= base_increment;
    else                 f.wide  = ratio * (high - low);

    arith->free = f;

    renormalize_and_write_if_needed( arith );
}


bool arith_Decode_Bit(   Arith* arith,   ulong mid,   ulong total   ) {
    /* Read Arithmetic-Encoding.doc if you find this function mysterious! */

    /***********************************************************/
    /*   The naive coding for our basic computation here is:   */
    /*                                                         */
    /*       bit   =   (base / (wide/total)) >= mid;           */
    /*                                                         */
    /*   Relative to the above, we eliminate one divide:       */
    /***********************************************************/

    Free f = renormalize_and_read_if_needed( arith );

    ulong r  = (f.wide / total) * mid;
    bool bit =  f.base >= r;

    if (bit) {    f.base -= r;   f.wide -= r; }
    else     {                   f.wide  = r; }

    arith->free = f;

    return bit;
}

