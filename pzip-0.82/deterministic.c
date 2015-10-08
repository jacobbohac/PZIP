/****************************************************************************************/
/* ``A context is defined to be "deterministic" when it gives only one                  */
/*   prediction.  We have found in experiments that for such contexts the               */
/*   obeserved frequency of the novel characters is much lower than expected            */
/*   based on a uniform prior distribution.  This can be exploited by using             */
/*   such contexts for prediction.  The strategy that we recommend is to                */
/*   choose the shortest deterministic prediction.  If there is no                      */
/*   deterministic context, then the longest context is chosen instead.''               */
/*                                                                                      */
/*    -- Unbounded Length Contexts for PPM                                              */
/*       John G Cleary, W J Teahan, Ian H Witten                                        */
/*       1996  10 pages                                                                 */
/*       http://www.cs.waikato.ac.nz/~ml/publications/1995/Cleary-Teahan-Witten-ppm.pdf */
/****************************************************************************************/

/*************************************************************************/
/*                                                                       */
/*                 "Sometimes I sits and thinks.                         */
/*                  And sometimes I just sits."                          */
/*                                -- Anon                                */
/*                                                                       */
/* This module constitutes a special-sauce supercharger to the regular   */
/* Partial Pattern Match code, picking off and handling a few special    */
/* cases which it can do better than can vanilla PPM.                    */
/*                                                                       */
/* In particular, we watch for situations where some suffix of the       */
/* current input-to-date has appeared before in the input, but           */
/* always followed by one particular symbol.                             */ 
/*                                                                       */
/* pzip.c gives us "right of first refusal" on each symbol:  On the      */
/* few occasions when we think we can do a better job than the vanilla   */
/* PPM code we take over the encoding process.  Most of the time, we     */
/* "just sits".                                                          */
/*************************************************************************/


#include "deterministic.h"
#include "det_escape.h"
#include "inc.h"
#include "pool.h"
#include "intmath.h"
#include "node.h"
#include "config.h"

/*******

  @@ lru the contexts!

 *********/

#define DETERMINISTIC_MAX_MATCH_LEN      (1024)
#define DETERMINISTIC_MAX_NODES_TO_VISIT  (100)

#define NODE_ARRAY_SIZE           (1<<18)             /* A 256k window. */

#define DO_ADDNODE_ON_SUCCESS

#define HASH_MASK       (0xFFFF)
#define HASH_SIZE       (HASH_MASK + 1 )
#define HASH(x)         ((((x)>>11) ^ (x) )&HASH_MASK)

typedef struct Deterministic_Node       Deterministic_Node;
typedef struct Deterministic_Context    Deterministic_Context;

/*************************************************************/
/* A PZIP Trie can be at most PZIP_ORDER (8) 'child' */
/* links deep, so the 'child' links of order 7 leaf Contexts */
/* must always be NULL.                                      */
/*                                                           */
/* We take advantage of this observation to re-use the       */
/* 'child' pointer of such leaf Contexts to point to our     */
/* Deterministic_Context instances.  Each such instance      */
/* represents all "deterministic" contexts ending in a       */
/* given 12-byte suffix.                                     */
/*                                                           */
/* We count Deterministic_Context matches and escapes        */
/* much as we do for regular Context instances.              */
/*                                                           */
/* In addition, we maintain (via 'node') a doubly linked     */
/* of our complete family of 12-byte-suffix-related          */
/* "deterministic" contexts, one Deterministic_Node each.    */
/*                                                           */
/* Each Deterministic_Node contains a pointer to the spot in */
/* the input buffer at which it ends ('input_ptr'), and the  */
/* minimum length of suffix match needed to make its         */
/* prediction unique -- "deterministic" -- ("min_len").      */
/*                                                           */
/* The prediction of each such node is of course available   */
/* as *node->input_ptr: The byte following it in the input.  */
/*************************************************************/


struct Deterministic_Context {
    Node node;                /* All Deterministic_Nodes of this context, doubly linked. */
    uint matches_seen;
    uint escapes_seen;
};

struct Deterministic_Node {
    Node   node;              /* Must be at head! */
    uword  min_len;           /* Match must be at least this long to be unique. */
    ubyte* input_ptr;
};

struct Det {
    Pool*    deterministic_context_pool;
    Escape*  escape;

    Deterministic_Node node[ NODE_ARRAY_SIZE +1 ];
    uint     node_cursor;

    Deterministic_Node*    next_node;

    /* Stuff saved by Encode/Decode for Update. (Ick!!) */
    Deterministic_Context* cached_deterministic_context;
    Deterministic_Node*    cached_node;
    uint                   cached_match_len;
    uint                   longest_match_len;
};


Det* deterministic_Create( void ) {

    Det* self = new( Det );

    self->deterministic_context_pool = pool_Create( sizeof( Deterministic_Context ), 100*1024, 100*256 );

    self->escape      = escape_Create();
    self->node_cursor = 0;

    {   int  i;
        for (i = NODE_ARRAY_SIZE;   i --> 0;)   node_Init( &self->node[i] );
    }

    return self;
}

void deterministic_Destroy(   Det* self   ) {
    assert( self );

    pool_Destroy(     self->deterministic_context_pool   );
    escape_Destroy(   self->escape                       );
    destroy(          self                               );
}

static Deterministic_Node* alloc_deterministic_node(   Det* self   ) {
    Deterministic_Node* node = &self->node[ self->node_cursor++ ];
    if (self->node_cursor == NODE_ARRAY_SIZE) {
        self->node_cursor = 0;
    }
    node_Cut( node );
    return    node;
}

static Deterministic_Node* next_deterministic_node(   Det* self,   Deterministic_Node* node   ) {
    ++node;
    if (node == &self->node[ NODE_ARRAY_SIZE ]) {
        node  = &self->node[               0 ];
    }
    return node;
}

static Deterministic_Context* fetch_or_make_deterministic_context(   Det* det,   Context* context   ) {

    /**************************************************************/
    /* If 'context' already has a Deterministic_Context child, we */
    /* can just re-use it, otherwise create a new one.            */
    /**************************************************************/

    if (context->child) {
        return (Deterministic_Context*) context->child;
    } else {
        Deterministic_Context* dc = pool_Get_Hunk( det->deterministic_context_pool );
        dc->escapes_seen  = 1;
        dc->matches_seen  = 1;
        node_Init( &dc->node );
        context->child = (void*) dc;
        return dc;
    }
}

static Deterministic_Node* add_node_to_context(   Det* self,   Context* context,   ubyte* input_ptr,   uint min_len   ) {

    Deterministic_Context* dc   = fetch_or_make_deterministic_context( self, context );
    Deterministic_Node*    node = alloc_deterministic_node( self );

    node_Add( &dc->node, node );

    node->min_len   = max( min_len, DETERMINISTIC_MIN_ORDER /* == 24 */ );
    node->input_ptr = input_ptr;

    return node;
}

void deterministic_Update(   Det* self,   ubyte* input_ptr,   int symbol,   Context* context   ) {

    /* We get called on each char */
    /* in the file in succession. */

    Deterministic_Node* node = self->cached_node;   /* Kept from last encode/decode call. */
    self->next_node = NULL;

    if (node) {
        assert( self->cached_deterministic_context );

        if (*node->input_ptr == symbol) {

            ++ self->cached_deterministic_context->matches_seen;

            /* I don't quite understand this hack, but */
            /* it makes us run more than twice as fast */
            /* and apparently compress a little better */
            /* to boot:                                */
            self->next_node = next_deterministic_node( self, node );

        } else {

            ++ self->cached_deterministic_context->escapes_seen;

            assert( self->cached_match_len >= node->min_len );

            node->min_len = self->cached_match_len + DETERMINISTIC_MIN_LEN_INC /* == 2 */;
        }
    }

    add_node_to_context( self, context, input_ptr, self->longest_match_len +1 );
}

static int longest_common_suffix(   ubyte* p,   ubyte* q,   ubyte* input_buf   ) {

    /*********************************************/
    /* p and q are both pointers into input_buf. */
    /* Compute and return the length of their    */
    /* longest common suffix.                    */
    /*                                           */
    /* NB: To avoid O(N**2) slowdown on a file   */
    /* which endlessly repeats one symbol, we    */
    /* arbitrarily stop the match at len 1024.   */
    /*********************************************/

    /***************************************************/
    /* A tiny efficiency tweak:   We only get here if  */
    /* p and q are already known to share a suffix at  */
    /* least 12 bytes long, so we start our compare    */
    /* at the 13th byte back:                          */
    /***************************************************/
    p -= 13;
    q -= 13;

    {   int len     = 0;
        int max_len = min(   p - input_buf,   q - input_buf   );
        max_len     = min(   max_len,   DETERMINISTIC_MAX_MATCH_LEN /* == 1024 */ );
        while (*p-- == *q--) {
            if (++len >= max_len)   break;
        }
        return len + 12;   /* Count the 12 known-to-match bytes too! */
    }
}

static void  find_best_node(   Det* self,   Deterministic_Context* dc,   ubyte* input_ptr,   ubyte* input_buf   ) {

    if (!dc) {
        /* @@ 2002-05-14 cbloom bug fix  */
        self->cached_deterministic_context = NULL;
        self->cached_node        = NULL;
        self->longest_match_len  = 0;
        self->cached_match_len   = 0;
        return;
    }

    /* Our logic assumes 24 bytes of input history, so */
    /* just sit out the first 24 bytes of the file:    */
    if (input_ptr - input_buf < DETERMINISTIC_MIN_ORDER /* == 24 */)   return;

    /*******************************************************************/
    /* The John G Cleary / W J Teahan / Ian H Witten top-of-file quote */
    /* to the contrary, we get the best results (on the Calgary Corpus */
    /* at least) if we pick the longest not shortest "deterministic"   */
    /* context match.                                                  */
    /*******************************************************************/

    {   Deterministic_Node* best_node   = NULL;
        uint                best_len    = 0;
        uint                longest_len = 0;

        uint nodes_visited = 0;
        Deterministic_Node* node;
        for (node = node_Next(&dc->node);   node != (Deterministic_Node*)&dc->node;   node = node_Next(node)) {

            uint len = longest_common_suffix(   input_ptr,   node->input_ptr,   input_buf   );

            longest_len = max( longest_len, len );

            if (len >= node->min_len
            &&  len > best_len
            ){
                best_len  = len;
                best_node = node;
            }

            /* Take out some insurance against pathological cases: */
            if (++nodes_visited == DETERMINISTIC_MAX_NODES_TO_VISIT /* == 100 */)   break;
        }

        self->cached_deterministic_context = dc;
        self->cached_node                  = best_node;

        self->longest_match_len = longest_len;
        self->cached_match_len  = best_len;

        assert( best_len >= DETERMINISTIC_MIN_ORDER || ! best_node );
    }
}

static void find_match(   Det* self,   ubyte* input_ptr,   ubyte* input_buf,   Context* context   ) {

    if (!self->next_node) {

        self->cached_deterministic_context = NULL;
        self->cached_node                  = NULL;

        find_best_node(   self,   (Deterministic_Context*) context->child,   input_ptr,   input_buf   );

    } else {

        self->cached_deterministic_context = (Deterministic_Context*) context->child;

        if (!self->cached_deterministic_context) {

            find_best_node( self, (Deterministic_Context*) context->child, input_ptr, input_buf );

        } else {

            self->cached_node = self->next_node;
            self->cached_match_len ++;
            self->longest_match_len = max(   self->longest_match_len,   self->cached_match_len   );

            if (self->cached_match_len >= 64) {

                /* Force it to accept this match: */
                self->cached_node->min_len = min(   self->cached_node->min_len,   self->cached_match_len   );

            } else {

                if (self->cached_match_len < self->cached_node->min_len) {
                    find_best_node( self, (Deterministic_Context*) context->child, input_ptr, input_buf );
                }
            }
        }
    }
}

bool deterministic_Encode(   Det* self,   Arith* arith,   ubyte* input_ptr,   ubyte* input_buf,   int symbol,   Excluded_Symbols* excl,   Context* context   ) {

    find_match( self, input_ptr, input_buf, context );

    if (!self->cached_node)   return FALSE;

    {   int  count      =  self->cached_deterministic_context->matches_seen;
        int  prediction = *self->cached_node->input_ptr;

        if (self->cached_match_len >= 64)   count = 99999;

        assert( excluded_symbols_Is_Empty( excl ) );

        {   bool match = (symbol == prediction);

            escape_Encode( self->escape, arith, getulong( input_ptr -4 ),    1,   count,   context->followset_size, !match );
            /*                                            key             escC    totSymC  numParentSyms             escape */

            excluded_symbols_Add( excl, prediction );

            return match;
        }
    }
}

bool deterministic_Decode(   Det* self,   Arith* arith,   ubyte* input_ptr,   ubyte* input_buf,   int* psymbol,   Excluded_Symbols* excl,   Context* context   ) {

    find_match( self, input_ptr, input_buf, context );

    if (!self->cached_node)   return FALSE;


    {   int  count  =  self->cached_deterministic_context->matches_seen;
        int  symbol = *self->cached_node->input_ptr;

        if (self->cached_match_len >= 64)   count = 99999;

        assert( excluded_symbols_Is_Empty( excl ) );

        {   bool match = ! escape_Decode( self->escape, arith, getulong( input_ptr -4 ), 1, count, context->followset_size );

            excluded_symbols_Add( excl, symbol );

            *psymbol = symbol;

            return match;
        }
    }
}
