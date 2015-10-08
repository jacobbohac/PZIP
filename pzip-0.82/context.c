#include "context.h"
#include "config.h"
#include "pool.h"

    /**************************************/
    /* SEE ALSO the comments in context.h */
    /**************************************/


/* The set of symbols which have been observed to follow */
/* a given context are stored in a singly linked list    */
/* hung off Context.followset:                           */
struct Followset_Node {
    Followset_Node* next;      /* Next node in linklist else NULL. */
    uword           symbol;    /* The symbol (letter/byte) proper. */
    uword           count;     /* Number of times symbol has been  */
};                             /* seen in this context.            */


/*************
Context : Just track seen symbols, counts & escapes; no coding here

        The only fudgy bit here is the Update_See:
        We actually do this for the *2nd* time on
        contexts we code from, since EncodeEscape
        does an additional Adjust for us however,
        the states we update here may not be the
        same ones we used, since the actual coding
        & LOE use excluded states..

*************/


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


//-----------------------------------------
// static pools
static Pool* context_pool            = NULL;
static int   context_pool_count      = 0;
static Pool* context_node_pool       = NULL;
static int   context_node_pool_count = 0;
//-----------------------------------------


Context* context_Create(   Context* parent,   uint key   ) {

    Context* context = pool_Auto_Get_Hunk( &context_pool, &context_pool_count, sizeof(Context) );

    node_Init(  context                      );
    node_Init( &context->least_recently_used );

    context->followset  = NULL;
    context->parent     = parent;
    context->key        = key;
    context->order      = parent   ?   parent->order +1   :   0;

    if (parent) {
        if (parent->child)   node_Add( parent->child, context );
        else                 parent->child = context;
    }       

    return context;
}

void context_Destroy_All_Contexts( void ) {
    pool_Auto_Destroy( &context_pool,      &context_pool_count      );
    pool_Auto_Destroy( &context_node_pool, &context_node_pool_count );
}

static void context_destroy( Context* context ) {
    Followset_Node* symbols;
    Followset_Node* next;

    if (!context)   return;

    node_Cut( context                       );
    node_Cut( &context->least_recently_used );

    
    for (symbols = context->followset;   symbols;   symbols = next) {
        next = symbols->next;
        pool_Auto_Free_Hunk( &context_node_pool, &context_node_pool_count, symbols );
    }
        
    pool_Auto_Free_Hunk( &context_pool, &context_pool_count, context );
}

static void context_delete(   Context* self,   Trie* trie   ) {

    /* We are called (only) when recycling */
    /* a least-recently-used Context to    */
    /* make room for a new one.            */

    assert( ! self->parent || self->parent->order == self->order -1 );
    assert( self->order == PZIP_ORDER || ! self->child  || self->child->order  == self->order +1 );
    assert( ((Context*) node_Next(self))->order == self->order );

    /* Recursively delete any children: */
    if (self->child) {
        if (self->order == PZIP_ORDER /* == 8 */) {
            /* self->child is a Deterministic ! */
            /* deterministic_Delete_Nodes(         */
            self->child = NULL;       /* Just let the deterministic lru clean it up later */
        } else {
            do {
                assert( self->child->order  == self->order +1 );
                assert( self->child->parent == self           );
                context_delete( self->child, trie );
            } while (self->child);
        }
    }

    /* Don't leave self->parent->child pointing */
    /* to us, since we're no longer valid:      */
    if (self->parent)    {
        assert( self->parent->order == self->order -1 );

        if (self->parent->child == self) {

            /* Parent points to me. Try to get a sibling: */
            self->parent->child = node_Next( self );
            if ( self->parent->child == self ) {

                /* No siblings, so nullify: */
                self->parent->child = NULL;
            }
        }
    }

    assert( ! self->parent || self->parent->order == self->order -1 );
    assert( ! self->child );
    assert( ((Context*)node_Next(self))->order == self->order );

    context_destroy( self ); /* Takes care of removing us from sibling and lru linklists. */

    -- trie->lru_context_count;
}

static Context* find_child_matching_key(   Context* self,   ulong key   ) {

    /*****************************************/ 
    /* Run circularly around the linklist of */
    /* our children. Return the one matching */
    /* 'key', if any, else NULL:             */
    /*****************************************/ 

    Context* c = self->child;
    if (c) {
        do {
            if (c->key == key) {

                /* Found it -- return it! */

                /* Before returning, move our context to front */
                /* of linklist.  Over time, this will sort the */
                /* most frequently accessed contexts to the    */
                /* front of the linklist, reducing average     */
                /* search time:                                */
                if (c != self->child) {  /* xxx this is move-almost-to-front */
                    node_Cut( c );
                    node_Add( self->child, c );
                }

                return c;
            }
            c = node_Next( c );
        } while (c != self->child);
    }
    return NULL;
}

static void maybe_halve_counts( Context* self ) {

    /* To keep the logic in our arithmetic encoder  */
    /* from overflowing, we must periodically halve */
    /* the appearance counts in our follow set:     */

    if (self->total_symbol_count < CONTEXT_COUNT_HALVE_THRESHOLD)   return;

    /* Recompute our symbol statistics */
    /* from scratch as we go:          */ 
    self->followset_size    = 0;      /* Number of symbols in the followset */
    self->total_symbol_count = 0;      /* Sum of all follow->counts.          */
    self->max_count      = 0;      /* Max of all follow->counts.          */

    {   Followset_Node*  node;
        Followset_Node** node_ptr;
    
        /* Over all symbols which have followed this context: */
        for (node_ptr = &self->followset;   (node = *node_ptr) != NULL;   ) {

            /* Halve the appearance count for the symbol: */
            node->count >>= 1;

            if (node->count == 0) {

                /* The count has gone to zero, */
                /* so delete the node:         */ 
                *node_ptr = node->next;        /* Remove node from our linklist. */
                pool_Auto_Free_Hunk( &context_node_pool, &context_node_pool_count, node );

            } else {

                if (node->count <= CONTEXT_SYMBOL_INC_NOVEL) {
                    node->count  = CONTEXT_SYMBOL_INC_NOVEL +1;
                }

                self->total_symbol_count += node->count;

                ++ self->followset_size;

                self->max_count = max( self->max_count, node->count );

                node_ptr = &node->next;
            }
        }
    }
        
    self->escape_count = (self->escape_count >> 1) +1;
}


void context_Update(   Context* self,   int symbol,   ulong key,   See* see,   int coded_order   ) {

    /* <> could track the 'if I had coded' entropy here */

    /*************************************************************/
    /* 'symbol' has appeared immediately following this context; */
    /* Update the followset statistics to reflect this fact.    */
    /*************************************************************/

    assert( ! self->parent || self->parent->order == (self->order - 1) );
    assert( self->order == PZIP_ORDER || ! self->child  || self->child->order  == (self->order + 1) );
    assert( ((Context *)node_Next(self))->order == self->order );

    if (self->order < coded_order)   return;

    {   Followset_Node** last;
        Followset_Node*  node;
        bool             escape = TRUE;

        maybe_halve_counts( self );

        /* Check first to see if we already */
        /* have 'symbol' in our followset:  */
        for (last = &self->followset;   node = *last;   last = &node->next) {

            if (node->symbol == symbol) {

                /* Move 'node' to front of linklist to reduce */
                /* average search time in future.  (This cut  */
                /* pzip runtime by 12.5% when I added it.)    */
                *last            = node->next;              
                node->next       = self->followset;
                self->followset = node;

                if (node->count <= CONTEXT_SYMBOL_INC_NOVEL) {

                    self->escape_count       -= CONTEXT_ESCP_INC;
                    node->count              += CONTEXT_SYMBOL_INC - CONTEXT_SYMBOL_INC_NOVEL;
                    self->total_symbol_count += CONTEXT_SYMBOL_INC - CONTEXT_SYMBOL_INC_NOVEL;

                    if (self->escape_count < 1) {
                        self->escape_count = 1;
                    }
                }

                node->count              += CONTEXT_SYMBOL_INC;
                self->total_symbol_count += CONTEXT_SYMBOL_INC;

                escape = FALSE;
                break;
            }
        }

        if (escape) {

            /* Add a new node to our follow set: */
            node             = pool_Auto_Get_Hunk( &context_node_pool, &context_node_pool_count, sizeof( Followset_Node ) );
            node->next       = self->followset;
            self->followset = node;

            node->symbol   = symbol;
            node->count    = CONTEXT_SYMBOL_INC_NOVEL;

            self->total_symbol_count += CONTEXT_SYMBOL_INC_NOVEL;       

            if (self->escape_count < CONTEXT_ESCAPE_MAX) {
                self->escape_count += CONTEXT_ESCP_INC;
            }

            ++ self->followset_size;
        }

        self->max_count = max( self->max_count, node->count );

        if (!see) {
            self->see_state = NULL;
        } else {
            // Note that this may or may not be 
            // the same state that we coded from, because
            // of exclusions and such
            see_Adjust_State( see, self->see_state, escape );
            self->see_state = see_Get_State(   see,   self->escape_count,   self->total_symbol_count,   key,   self   );
        }
    }
}

Followset_Stats context_Get_Followset_Stats_With_Given_Symbols_Excluded(   Context* self,   Excluded_Symbols* excl   ) {

    /**********************************************/
    /* Gather follow-set stats for Context 'self' */
    /* excluding symbols in 'excl'. (We use this  */
    /* to make next-char probability predictions  */
    /* when the symbols in 'excl' are known to be */
    /* ruled out by other considerations.)        */
    /**********************************************/

    Followset_Stats stats;

    if (excluded_symbols_Is_Empty( excl )) {

        /* If there -are- no excluded symbols, */
        /* we can save time by using our       */
        /* precomputed follow-set stats:       */
        stats.total_count   = self->total_symbol_count;
        stats.max_count     = self->max_count;
        stats.escape_count  = self->escape_count;

    } else {

        Followset_Node * n;

        // escape from un-excluded counts
        //      also count the excluded escape symbols, but not as hard
        // rig up the counding so that 1 excluded -> 1 final count

        // helped paper2 2.193 -> 2.188 bpc !!
        // you can get 0.001 bpc by tweaking all these constants :

        // these counts make 1 excl -> 1, 2 -> 2, and 3 -> 2 , etc.
        //      so for low-escape contexts, we get the same counts, and for low-orders we get
        //      much lower escape counts

        stats.max_count    = 0;
        stats.total_count  = 0;
        stats.escape_count = CONTEXT_EXCLUDED_ESCAPE_INIT;

        for (n = self->followset;   n;   n = n->next) {

            if (excluded_symbols_Contains( excl, n->symbol ) ) {

                if (n->count <= CONTEXT_SYMBOL_INC_NOVEL) {
                    stats.escape_count += CONTEXT_EXCLUDED_ESCAPE_EXCLUDEDINC;
                }

            } else {

                stats.total_count += n->count;

                if (n->count > stats.max_count) {
                    stats.max_count = n->count;
                }

                if (n->count <= CONTEXT_SYMBOL_INC_NOVEL) {
                    stats.escape_count += CONTEXT_EXCLUDED_ESCAPE_INC;
                }
            }
        }

        stats.escape_count >>= CONTEXT_EXCLUDED_ESCAPE_SHIFT;
    }               

    return stats;
}

// If a symbol of count < Novel is excluded, should we subtract from the escape?
// I think not, since DONT_SEE_EXCLUDED failed

bool context_Encode(   Context* self,   Arith* arith,   Excluded_Symbols* excl,   See* see,   ulong key,   int symbol   ) {

    assert( ! excluded_symbols_Contains( excl, symbol ) );

    if (self->total_symbol_count == 0)   return FALSE;

    assert( self->total_symbol_count > 0 );

    {   Followset_Stats stats = context_Get_Followset_Stats_With_Given_Symbols_Excluded( self, excl );

        if (stats.total_count == 0)   return FALSE;   /* No chars unexcluded. */

        {   int low  = 0;
            int high = 0;
            Followset_Node* n;
            for (n = self->followset;   n;   n = n->next) {
                assert( n->count > 0 );

                if (!excluded_symbols_Contains( excl, n->symbol ) ) {

                    if (n->symbol == symbol) {   high = low + n->count;   }   /* Found it! */ 
                    else if (high == 0)      {   low += n->count;         }

                    excluded_symbols_Add( excl, n->symbol );
                }
            }

//            assert( stats.total_count < arith->prob_max && high <= stats.total_count );

            {   See_State*    ss;
                if ( stats.escape_count > stats.total_count ) ss = NULL;
                else                                    ss = see_Get_State( see, stats.escape_count, stats.total_count, key, self );

                if (high) {
                    /* Found it: */
                    see_Encode_Escape( see, arith, ss, stats.escape_count, stats.total_count, FALSE );
                    arith_Encode_1_Of_N( arith, low, high, stats.total_count );
                    return TRUE;
                } else {
                    see_Encode_Escape( see, arith, ss, stats.escape_count, stats.total_count, TRUE );
                    return FALSE;
                }
            }
        }
    }
}

bool context_Decode(   Context* self,   Arith* arith,   Excluded_Symbols* excl,   See* see,   ulong key,   int* psymbol   ) {

    if (self->total_symbol_count == 0)    return FALSE;

    {   Followset_Stats stats = context_Get_Followset_Stats_With_Given_Symbols_Excluded( self, excl );

        if (stats.total_count == 0)   return FALSE;   /* No chars unexcluded. */

        {   See_State *ss;

            if (stats.escape_count > stats.total_count)   ss = NULL;
            else                                    ss = see_Get_State( see, stats.escape_count, stats.total_count, key, self );

            if (see_Decode_Escape( see, arith, ss, stats.escape_count, stats.total_count ) )	{
                Followset_Node* n;
                for (n = self->followset;   n;   n = n->next) {
                    excluded_symbols_Add( excl, n->symbol );
                }
                return FALSE;
            }

//            assert( stats.total_count < arith->prob_max );

            {   int got = arith_Get_1_Of_N( arith, stats.total_count );
                int low = 0;
                Followset_Node* n;
                for (n = self->followset;   n;   n = n->next) {
                    assert( got >= low );
                    if (!excluded_symbols_Contains( excl, n->symbol )) {
                        int high = low + n->count;
                        if (got < high) {
                            /* Found it: */
                            arith_Decode_1_Of_N( arith, low, high, stats.total_count );
                            *psymbol = n->symbol;
                            return TRUE;
                        }
                        low = high;
                    }
                }
            }
        }
    }

    assert(0);   /* !! Should not get here! */

    return FALSE;
}


Trie* trie_Create( void ) {

    Trie* trie = new( Trie );

    trie->order0 = context_Create( NULL, 0 );

    {   uint i;
        for (i = 256;   i --> 0;   ) {
            trie->order1[i] = context_Create( trie->order0, i );
        }
    }

    node_Init( &trie->least_recently_used );

    trie->lru_context_count = 0;
    trie->max_lru_contexts  = (PZIP_TRIE_MEGS /* == 72 */ * 1024 * 1024) / sizeof( Context );

    return trie;
}

void trie_Destroy( Trie* trie ) {

    /* Note that context_Destroy_All_Contexts recycles all our  */
    /* Context instances en masse, so we don't need to do that: */
    destroy( trie );
}

static void maybe_recycle_least_recently_used_context( Trie* self ) {

    if (self->lru_context_count >= self->max_lru_contexts) {
        
        Node* last = node_Cut_Tail( &self->least_recently_used );        assert(last);
        Context* to_die = (Context*)( (ulong)last - (ulong)(&((Context*)0)->least_recently_used) );

        assert( ! to_die->parent || to_die->parent->order == (to_die->order - 1) );
        assert( to_die->order == PZIP_ORDER || ! to_die->child  || to_die->child->order  == (to_die->order + 1) );
        assert( ((Context *)node_Next(to_die))->order == to_die->order );
        assert( to_die->child != to_die->parent );

        context_delete( to_die, self );
    }
}

static Context* mark_new_context_as_most_recently_used(   Trie* self,   Context* context   ) {

    node_Add( &self->least_recently_used, &context->least_recently_used );

    ++ self->lru_context_count;

    maybe_recycle_least_recently_used_context( self );

    return context;
}

static Context* mark_old_context_as_most_recently_used(   Trie* self,   Context* context   ) {
    node_Cut( &context->least_recently_used );
    node_Add( &self->least_recently_used, &context->least_recently_used );
    return context;
}

static Context* find_or_create(   Trie* self,   Context* context,   ulong key   ) {

    /* Find and return the child of 'context' with   */
    /* child->key == key.  If none exists, make one: */

    {   Context* pre_existing = find_child_matching_key( context, key );
        if (pre_existing)  return mark_old_context_as_most_recently_used( self, pre_existing );

        return   mark_new_context_as_most_recently_used(   self,   context_Create( context, key )   );
    }
}

Contexts trie_Get_Active_Contexts(   Trie* self,   ubyte* input_so_far   ) {

    /*****************************************/
    /* As we compress the file byte by byte, */
    /* one of the first steps in processing  */
    /* any given byte is to locate the set   */
    /* of "active" Contexts -- those which   */
    /* match the current suffix of the input */
    /* stream.                               */
    /*                                       */
    /* For example, if the input so far were */
    /* "abracadabra", we would have          */
    /*            ""  (order 0 model)        */
    /*           "a"  (order 1 model)        */
    /*          "ra"  (order 2 model)        */
    /*         "bra"                         */
    /*        "abra"                         */
    /*         ...                           */
    /* as the currently active Contexts.     */
    /* (Plus the order -1 Context.)          */
    /*                                       */
    /* Our job in this function is to find   */
    /* (or create!) the set of active        */
    /* contexts and return them to our       */
    /* caller.                               */
    /*****************************************/
     

    /* Start by packing recent input into */
    /* a keys[] array for convenience:    */
    ulong keys[   PZIP_ORDER +1 ];
    keys[ 1 ] = input_so_far[ -1 ];
    keys[ 2 ] = input_so_far[ -2 ];
    keys[ 3 ] = input_so_far[ -3 ];
    keys[ 4 ] = input_so_far[ -4 ];
    keys[ 5 ] = input_so_far[ -5 ];
    /* A little hack:  We pack more than one byte  */
    /* of history into our later keys, effectively */
    /* extending our horizon a bit farther back in */
    /* time.  This is good for about 0.004 bpc:    */
    keys[ 6 ] = input_so_far[ -6 ]
              +(input_so_far[ -7 ] <<  8)
              +(input_so_far[ -8 ] << 16);
    keys[ 7 ] = getulong( input_so_far -12 );
    keys[ 8 ] = getulong( input_so_far -16 );

    /* Finding the right order0 Context is easy, since */
    /* there's only one. :)  Finding the right order1  */
    /* Context isn't much harder:                      */
    {   Contexts contexts;
        contexts.c[0] = self->order0;
        contexts.c[1] = self->order1[ keys[1] ];

        /* We find the the remaining active Contexts   */
        /* by searching the children of our order1     */
        /* Context for the one with a 'key' field      */
        /* matching our input-derived one, then        */
        /* recursively on down to its children &tc:    */
        {   uint i;
            for (i = 2;   i <= PZIP_ORDER;  i++) {

                contexts.c[i] = find_or_create( self, contexts.c[i-1], keys[i] );

                /* Sanity checks for debugging purposes: */
                {   Context* context = contexts.c[i];
                    assert(   ! context->parent   ||   context->parent->order == context->order -1   );
                    assert( context->order == PZIP_ORDER || ! context->child  || context->child->order == context->order +1 );
                    assert( (( Context*) node_Next(context) )->order == context->order );
                } 
            }
        }
        return contexts;
    }
}

