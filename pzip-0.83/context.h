#ifndef CONTEXT_H
#define CONTEXT_H

#include "inc.h"
#include "config.h"
#include "node.h"
#include "see.h"
#include "excluded_symbols.h"
#include "arithmetic-encoding.h"
#include "deterministic.h"
#include "pool.h"

#ifndef DEFINED_CONTEXT
typedef struct Context Context;
#define DEFINED_CONTEXT
#endif

typedef struct Followset_Node Followset_Node;

/***

    Siblings, parent & child make this a 3-branch tree :

    (n->child->parent == n) ; parent is just redundant

    to find the child you want, step onto the child pointer, then
     walk the sisters till you get the ->key you seek

    All siblings have the same parent

    (semi-hack : the child pointer of the deepest node is actually a Det context!)

***/

/* We have to spill our guts to the world here   */
/* because context-trie breaks encapsulation. :( */

/*******************************************************/
/* In file compression by Partial Pattern Match (PPM), */
/* a 'context' is the N most recently seen symbols     */
/* (bytes) in the input stream, where N is typically   */
/* zero to four or so.                                 */
/*                                                     */
/* The idea is that if, say, 'u' appears much more     */
/* frequently after 'q', we can give is an especially  */
/* compact encoding in that context, and thus save     */
/* space.                                              */
/*                                                     */
/* Our job here and in context-trie is to implement,   */
/* store, index and manipulate such contexts.          */
/*                                                     */
/* Here's how it works.                                */
/*                                                     */
/* 'key' holds the most recent symbol (byte) in the    */
/* context.  If we'd just seen "abracadabra" in the    */
/* input stream, 'key' would be "a".                   */
/*                                                     */
/* The preceding characters in our context are found   */
/* by following our 'parent' pointerchain to its end,  */
/* collecting the 'key' values as we go.               */
/*                                                     */
/* Thus, if our context were "bra", we would have:     */
/*                                                     */
/*                         self->key == 'a'            */
/*                 self->parent->key == 'r'            */
/*         self->parent->parent->key == 'b'            */
/*     self->parent->parent->parent == NULL            */
/*                                                     */
/* Read the keys in reverse, and there's your "bra".   */
/*                                                     */
/* To navigate parent-to-child, we equip each Context  */
/* with a 'child' pointer pointing to an arbitrary     */
/* child;  The rest of our children are joined in a    */
/* circular doubly-linked list via a 'siblings' field. */
/*                                                     */
/* In the literature, a context with a parent-chain    */
/* of length zero is called an "Order 0 model", a      */
/* context with a parent pointer-chain of length one   */
/* is called an "Order 1 model", and so forth.         */
/*                                                     */
/* In other words, the "order" of a context is the     */
/* number of symbols in its suffix string, minus one.  */
/*                                                     */
/* To save constantly recomputing this number, we      */
/* store this explicitly in each context record in     */
/* its 'order' field.                                  */
/*                                                     */
/* As a special case, we have an "Order -1 model"      */
/* which represents the "context" containing no        */
/* symbol history at all.  It serves as a default      */
/* model which may be used when no other models apply, */
/* for example on the first byte of a file being       */
/* compressed.  (See the "order-1.[ch]" files.)        */
/*                                                     */
/* We track each context's "follow set":  The set of   */
/* symbols which have appeared following that context. */
/*                                                     */
/* For example, after seeing "abracadabra", the follow */
/* set of the context "a" is { 'b', 'c', 'd' }.        */
/*                                                     */
/* We also count how many times each symbol in the     */
/* follow set how many times it has appeared. This is  */
/* what allows us to predict which symbols are most    */
/* likely to appear next in the input stream in a      */
/* given context.                                      */
/*                                                     */
/* In the above example, after seeing "abracadabra",   */
/* the follow set of "a" with per-symbol counts is     */ 
/*    { 'b':2, 'c':1, 'd':1 }                          */
/* so we might predict that in future in this context, */
/* based on information to date, that 'b' is about     */
/* twice as probable as 'c' or 'd', which are equally  */
/* probable to appear.                                 */
/*                                                     */
/* Obviously, as we accumulate more information, our   */
/* our predictions improve accordingly.                */
/*                                                     */
/* We implement the follow set via the field           */
/* 'followset', which heads a singly-linked list of   */
/* Followset_Nodes, each containing fields for the    */
/* symbol, its appearance count, and a 'next' pointer. */      
/*                                                     */
/* To save time recomputing, we also maintain some     */
/* summary statistics of the follow set:               */
/*                                                     */
/*    symbol_count:   Number of symbols in follow set. */
/*    total_symbol_count: Sum of all 'count's in set.  */
/*    max_count:      Maximum of all 'count's in set.  */
/*                                                     */
/* We use 'escape_count' to track the number of novel  */
/* symbols seen following our Context.                 */
/*                                                     */
/* When we run out of space for new Contexts (relative */
/* to an arbitrary a prior limit PZIP_TRIE_MEGS), we   */
/* recycle the least-recently-used Context.            */
/*                                                     */
/*******************************************************/

/******************************************************************/
/* Trie is the master index to all our Context instances.         */
/* There is exactly one Trie instance in the system.              */
/*                                                                */
/* We search it once per symbol encoded/decoded to find the most  */
/* relevant Context(s) with which to encode the symbol.           */
/*                                                                */
/* Our 'order0' Context makes predictions based only on symbol    */
/* frequencies to date, ignoring the most recent input chars.     */
/*                                                                */
/* Our 'order1' Contexts make predictions based on one symbol of  */
/* input history, 'order 2' on two symbols, and so forth.         */
/*                                                                */
/* Our order 2 Contexts are hung off the 'order1' Contexts via    */
/* 'child' pointers.  Similarly, our order 3 Contexts are hung    */
/* off the 'child' pointers of the order 2 Contexts, and so on.   */
/*                                                                */
/* The order -1 Context, which makes fixed equiprobable symbol    */
/* predictions independent of input to date, is implemented       */
/* in order-1.[ch], and not explicitly dealt with in this module. */
/*                                                                */
/* If/when we run out of space for new Contexts, we recycle the   */
/* least-recently used Context:  The last three fields in the     */
/* Trie provide the state to support this.                        */
/******************************************************************/

struct Trie {
    Context*  order0;
    Context*  order1[ 256 ];

    Node      least_recently_used;
    uint      lru_context_count;
    uint      max_lru_contexts;
};
typedef struct Trie Trie;


/* The set of symbols which have been observed to follow */
/* a given context are stored in a singly linked list    */
/* hung off Context.followset:                           */
struct Followset_Node {
    Followset_Node* next;      /* Next node in linklist else NULL. */
    u16           symbol;    /* The symbol (letter/byte) proper. */
    u16           count;     /* Number of times symbol has been  */
};                             /* seen in this context.            */

typedef union {
    u64 u_64;
    u32 u_32;
    u16 u_16;
    u08 u_08;
} uany ;

struct Suffix {
    uany _0_to_7;            /* Up to eight bytes of suffix.        */
    uany _8_to_F;            /* Up to another eight bytes.          */
};
typedef struct Suffix Suffix;

struct Context {

    int      order;                     /* Length of 'parent' pointerchain.             */

    Context* parent;                   
    int      kids;

    Context* hashlink;                  /* Implements hash table chaining.              */

    Suffix   suffix;

    Followset_Node* followset;          /* One node for every symbol in follow set.     */ 
    int             followset_size;     /* Number of symbols in the followset           */
    int             total_symbol_count; /* Sum of all symbol's counts.                  */
    int             max_count;          /* Max of all 'follow->count's.                 */
    int             escape_count;       /* Count of novel symbols in follow set.        */

    Node            least_recently_used;

    See_State*      see_state;

    Deterministic_Context* det;
};

/* Return value for context_Get_Followset_Stats_With_Given_Symbols_Excluded(): */
typedef struct {
    int max_count;     /* Of all Followset_Nodes n in set, what is max n->count? */
    int total_count;   /* Sum over all Followset_Nodes n in set of n->count.     */
    int escape_count;  /* Roughly: Number of novel symbols seen in this context. */
} Followset_Stats;

void     context_Destroy_All_Contexts( void );       /* Global & naughty, but oh-so-fast */

void     context_Update(   Context* self,   int symbol,   u32 key,   See* see,   int coded_order  );

Followset_Stats context_Get_Followset_Stats_With_Given_Symbols_Excluded(   Context* self,   Excluded_Symbols* excl   );

/* Bools indicated coded vs. escaped: */
bool context_Encode(       Context* self,   Arith* arith,   Excluded_Symbols* excl,   See* see,   u32 key,   int   symbol  );
bool context_Decode(       Context* self,   Arith* arith,   Excluded_Symbols* excl,   See* see,   u32 key,   int* psymbol  );

/* A global type and variable to hold our active contexts du microsec: */
typedef struct {
    Context* c[ PZIP_ORDER +1 ];
} Contexts;
extern Contexts active_contexts;

/* There's only one Trie, so just publish it: */
extern Trie* trie;

Trie* trie_Create( void );

void trie_Destroy(                Trie* self );
void trie_Fill_Active_Contexts(   u08* input_ptr   );

Context* context_Is_Most_Recently_Used( Context* context );


#ifdef __GNUC__
extern inline Context* context_Is_Most_Recently_Used(   Context* context   ) {
    node_Cut( &context->least_recently_used );
    node_Add( &trie->least_recently_used, &context->least_recently_used );
    return context;
}
#endif /* __GNUC__ */

#endif // CONTEXTS_H
