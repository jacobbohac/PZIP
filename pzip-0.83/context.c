#include <stdio.h>
#include "context.h"
#include "config.h"
#include "pool.h"
#include "hash.h"

    /**************************************/
    /* SEE ALSO the comments in context.h */
    /**************************************/


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


/*****************************************/
/* static pools                          */

static Pool* context_node_pool       = NULL;
static int   context_node_pool_count = 0;

static Pool* pool        = NULL;
static int   pool_count  = 0;

/*                                       */
/*****************************************/

Contexts active_contexts;
Trie* trie;

static Context* context_create(   Suffix suffix,   int order   ) {

    Context* self = pool_Auto_Get_Hunk( &pool, &pool_count, sizeof(Context) );

    self->hashlink = NULL;
    self->parent   = NULL;
    self->order    = order;
    self->kids     = 0;

    node_Init( &self->least_recently_used );

    self->see_state  = NULL;

    self->suffix             = suffix;
    self->followset          = NULL;
    self->followset_size     = 0;
    self->total_symbol_count = 0;
    self->max_count          = 0;
    self->escape_count       = 0;

    switch (self->order) {
    case 0:
    case 1:        break;
    case 2:        hash_Note_Context_02( self, suffix );         break;
    case 3:        hash_Note_Context_03( self, suffix );         break;
    case 4:        hash_Note_Context_04( self, suffix );         break;
    case 5:        hash_Note_Context_05( self, suffix );         break;
    case 6:        hash_Note_Context_08( self, suffix );         break;
    case 7:        hash_Note_Context_12( self, suffix );         break;
    case 8:        hash_Note_Context_16( self, suffix );         break;
    default:
        assert( 0 && "bad order?!" );
    }

    return self;
}

void context_Destroy_All_Contexts( void ) {
    /* XXX we're not catching the subclass pools right now */
    pool_Auto_Destroy( &context_node_pool, &context_node_pool_count );
}

static void maybe_halve_counts( Context* self ) {

    /* To keep the logic in our arithmetic encoder  */
    /* from overflowing, we must periodically halve */
    /* the appearance counts in our follow set:     */

    if (self->total_symbol_count < CONTEXT_COUNT_HALVE_THRESHOLD)   return;

    /* Recompute our symbol statistics */
    /* from scratch as we go:          */ 
    self->followset_size     = 0;      /* Number of symbols in the followset */
    self->total_symbol_count = 0;      /* Sum of all follow->counts.         */
    self->max_count          = 0;      /* Max of all follow->counts.         */

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


void context_Update(   Context* self,   int symbol,   u32 key,   See* see,   int coded_order   ) {

    /* <> could track the 'if I had coded' entropy here */

    /*************************************************************/
    /* 'symbol' has appeared immediately following this context; */
    /* Update the followset statistics to reflect this fact.    */
    /*************************************************************/

    assert( ! self->parent || self->parent->order == (self->order - 1) );

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

bool context_Encode(   Context* self,   Arith* arith,   Excluded_Symbols* excl,   See* see,   u32 key,   int symbol   ) {

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

bool context_Decode(   Context* self,   Arith* arith,   Excluded_Symbols* excl,   See* see,   u32 key,   int* psymbol   ) {

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

    Suffix suffix;    suffix._0_to_7.u_64 = 0;    suffix._8_to_F.u_64 = 0;

    trie->order0 = context_create( suffix, 0 );

    {   uint i;
        for (i = 256;   i --> 0;   ) {
            suffix._0_to_7.u_64 = i;
            trie->order1[i] = context_create( suffix, /*order==*/1 );
            trie->order1[i]->parent = trie->order0;
            ++trie->order0->kids;  /* Not strictly necessary, but consistent. */
        }
    }

    node_Init( &trie->least_recently_used );

    trie->lru_context_count = 0;
#ifdef NORMAL
    trie->max_lru_contexts  = (PZIP_TRIE_MEGS /* == 72 */ * 1024 * 1024) / sizeof( Context );
#else
    /* Made constant to avoid annoying irrevant fluctuations in compression ratio: */
    trie->max_lru_contexts  = 1348169;
#endif

    return trie;
}

void trie_Destroy( Trie* trie ) {

    /* Note that context_Destroy_All_Contexts recycles all our  */
    /* Context instances en masse, so we don't need to do that: */
    destroy( trie );
}

int recycle_tries = 0;
int recycle_steps = 0;

static void context_delete(   Context* self   ) {

    /* We are called (only) when recycling */
    /* a least-recently-used Context to    */
    /* make room for a new one.            */

    assert( self->parent->order == self->order -1 );
    assert( !self->kids );

    --self->parent->kids;

    switch (self->order) {
    case 0:
    case 1:        break;
    case 2:        hash_Drop_Context_02( self );         break;
    case 3:        hash_Drop_Context_03( self );         break;
    case 4:        hash_Drop_Context_04( self );         break;
    case 5:        hash_Drop_Context_05( self );         break;
    case 6:        hash_Drop_Context_08( self );         break;
    case 7:        hash_Drop_Context_12( self );         break;
    case 8:        hash_Drop_Context_16( self );         break;
    default:
        assert( 0 && "bad order?!" );
    }

/*    node_Cut( &self->least_recently_used ); */
    
    {   Followset_Node* symbols;
        Followset_Node* next;
        for (symbols = self->followset;   symbols;   symbols = next) {
            next = symbols->next;
            pool_Auto_Free_Hunk(   &context_node_pool,   &context_node_pool_count,   symbols   );
        }
    }
        
    pool_Auto_Free_Hunk( &pool, &pool_count, self );

    -- trie->lru_context_count;
}

static inline Context* mark_as_most_recently_used(   Context* context   ) {
    node_Cut( &context->least_recently_used );
    node_Add( &trie->least_recently_used, &context->least_recently_used );
    return context;
}

static inline Context* mark_new_context_as_most_recently_used(   Context* context   ) {

    node_Add( &trie->least_recently_used, &context->least_recently_used );

    ++ trie->lru_context_count;

    /* Maybe recycle least recently used context: */
    if (trie->lru_context_count >= trie->max_lru_contexts) {
        
        Node* list = (Node*)&trie->least_recently_used;
        Node* node = list->prev;
        Context* to_die = (Context*)( (u32)node - (u32)(&((Context*)0)->least_recently_used) );
        assert( node != list );
        /* Only kill leafs, because that avoids */
        /* the problem of leaving dangling      */
        /* 'parent' pointers:                   */
        while (to_die->kids > 0) {
            node = node->prev;        assert( node != list );
            to_die = (Context*)( (u08*)node - (u08*)(&((Context*)0)->least_recently_used) );
        }
        assert( to_die->order >= 2);
        assert( to_die->parent->order == (to_die->order - 1) );
        node_Cut( node );
        context_delete( to_die );
    }

    return context;
}

static Context* create_kid(   Context* parent,   Suffix suffix   ) {
    Context* newkid = context_create( suffix, parent->order +1 );
    newkid->parent = parent;
    ++parent->kids;
    return mark_new_context_as_most_recently_used(   newkid   );
}

void trie_Fill_Active_Contexts(   u08* input_so_far   ) {

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
    /*         "bra"  (order 3 model)        */
    /*        "abra"  (order 4 model)        */
    /*       "dabra"  (order 5 model)        */
    /*      "adabra"  (order 6 model)        */
    /*     "cadabra"  (order 7 model)        */
    /*       ...            ...              */
    /* as the currently active Contexts.     */
    /* (Plus the order -1 Context.)          */
    /*                                       */
    /* Our job in this function is to find   */
    /* (or create!) the set of active        */
    /* contexts and return them to our       */
    /* caller.                               */
    /*                                       */
    /* In general, the lower-order Contexts  */
    /* we need will probably already exist,  */
    /* but the highest-order ones we will    */
    /* probably have to create (and will     */
    /* probably never use again).            */
    /*****************************************/
     

    /* A local synonym and cast for code clarity: */
    #undef  history
    #define history (u64)input_so_far

    Suffix suffix[ PZIP_ORDER +1 ];



    suffix[2]._0_to_7.u_16 = (history[ -1 ] << 0x00) | (history[ -2 ] << 0x08);
    suffix[3]._0_to_7.u_32 = suffix[2]._0_to_7.u_16  | (history[ -3 ] << 0x10);
    suffix[4]._0_to_7.u_32 = suffix[3]._0_to_7.u_32  | (history[ -4 ] << 0x18);
    suffix[5]._0_to_7.u_64 = suffix[4]._0_to_7.u_32  | (history[ -5 ] << 0x20);
    suffix[6]._0_to_7.u_64 = suffix[5]._0_to_7.u_64  | (history[ -6 ] << 0x28)
                                                     | (history[ -7 ] << 0x30)
                                                     | (history[ -8 ] << 0x38);
    suffix[7]._0_to_7.u_64 = suffix[6]._0_to_7.u_64;
    suffix[8]._0_to_7.u_64 = suffix[7]._0_to_7.u_64;
                                          
                                          
    suffix[7]._8_to_F.u_32 =  (history[ -9 ] << 0x00)
                           |  (history[-10 ] << 0x08)
                           |  (history[-11 ] << 0x10)
                           |  (history[-12 ] << 0x18);
    suffix[8]._8_to_F.u_64 =  suffix[7]._8_to_F.u_32
                           |  (history[-13 ] << 0x20)
                           |  (history[-14 ] << 0x28)
                           |  (history[-15 ] << 0x30)
                           |  (history[-16 ] << 0x38);
    #undef  history

    /* Finding the right order0 Context is easy, since */
    /* there's only one. :)  Finding the right order1  */
    /* Context isn't much harder:                      */
    active_contexts.c[0] = trie->order0;
    active_contexts.c[1] = trie->order1[ input_so_far[ -1 ] ];

    /* We find the the remaining active Contexts   */
    /* by searching the children of our order1     */
    /* Context for the one with a 'key' field      */
    /* matching our input-derived one, then        */
    /* recursively on down to its children &tc:    */
    /* Find and return the child of 'context' with   */
    /* child->key == key.  If none exists, make one: */

    #undef  a
    #define a active_contexts.c

    #ifdef THE_SIMPLE_TEXTBOOK_WAY

    {   Context* x = trie->order0;
        if (!x->kids || !(x = a[2] = hash_Find_Context_02( suffix[2] )))   x = a[2] = create_kid( a[1], suffix[2] );
        if (!x->kids || !(x = a[3] = hash_Find_Context_03( suffix[3] )))   x = a[3] = create_kid( a[2], suffix[3] );
        if (!x->kids || !(x = a[4] = hash_Find_Context_04( suffix[4] )))   x = a[4] = create_kid( a[3], suffix[4] );
        if (!x->kids || !(x = a[5] = hash_Find_Context_05( suffix[5] )))   x = a[5] = create_kid( a[4], suffix[5] );
        if (!x->kids || !(x = a[6] = hash_Find_Context_08( suffix[6] )))   x = a[6] = create_kid( a[5], suffix[6] );
        if (!x->kids || !(x = a[7] = hash_Find_Context_12( suffix[7] )))   x = a[7] = create_kid( a[6], suffix[7] );
        if (!x->kids || !(x = a[8] = hash_Find_Context_16( suffix[8] )))   x = a[8] = create_kid( a[7], suffix[8] );
    }
    mark_as_most_recently_used( a[2] );
    mark_as_most_recently_used( a[3] );
    mark_as_most_recently_used( a[4] );
    mark_as_most_recently_used( a[5] );
    mark_as_most_recently_used( a[6] );
    mark_as_most_recently_used( a[7] );
    mark_as_most_recently_used( a[8] );

    #else

    {   /**************************************************************************/
        /* The idea here is that it is faster to travel via 'parent' than 'child' */
        /* pointers, and that once we reach a node which has no kids, we can go   */
        /* ahead and create all the rest of the kids needed without further       */
        /* searching, so if we start in the middle (order5) and work from there   */
        /* towards both root and leaf, we can save a fair amount of time on the   */
        /* average.  Working out that idea logically produces the following code: */
        /**************************************************************************/

        /* Phase one:  Find all the pre-existing */
        /* nodes along our active-contexts path: */
        if       (a[5] = hash_Find_Context_05( suffix[5] )) {   a[4] = a[5]->parent;   a[3] = a[4]->parent;   a[2] = a[3]->parent;   goto tag;   }
        else if  (a[4] = hash_Find_Context_04( suffix[4] )) {                          a[3] = a[4]->parent;   a[2] = a[3]->parent;   goto five;  }
        else if  (a[3] = hash_Find_Context_03( suffix[3] )) {                                                 a[2] = a[3]->parent;   goto four;  }
        else if  (a[2] = hash_Find_Context_02( suffix[2] )) {                                                                        goto three; }
        goto two;
tag:    if (!a[5]->kids)   goto six;     if (!(a[6] = hash_Find_Context_08( suffix[6] )))   goto six;     
        if (!a[6]->kids)   goto seven;   if (!(a[7] = hash_Find_Context_12( suffix[7] )))   goto seven;   
        if (!a[7]->kids)   goto eight;   if (!(a[8] = hash_Find_Context_16( suffix[8] )))   goto eight;   
        goto done;

        /* Phase two: Create all the missing     */
        /* nodes along our active-contexts path: */
two:    a[2] = create_kid( a[1], suffix[2] );
three:  a[3] = create_kid( a[2], suffix[3] );
four:   a[4] = create_kid( a[3], suffix[4] );
five:   a[5] = create_kid( a[4], suffix[5] );
six:    a[6] = create_kid( a[5], suffix[6] );
seven:  a[7] = create_kid( a[6], suffix[7] );
eight:  a[8] = create_kid( a[7], suffix[8] );

        /* Phase three: Mark all the active      */
        /* contexts as recently used;            */
done:   mark_as_most_recently_used( a[2] );
        mark_as_most_recently_used( a[3] );
        mark_as_most_recently_used( a[4] );
        mark_as_most_recently_used( a[5] );
        mark_as_most_recently_used( a[6] );
        mark_as_most_recently_used( a[7] );
        mark_as_most_recently_used( a[8] );

        /* Now -that- is what I call "block-structured programming" :)      */
        /* That's also most of the 'goto's for my last 20 years od hacking. */
        /* But it speeded up pzip by 4% when switched on.                   */
    }                                                       

    #endif /* THE_SIMPLE_TEXTBOOK_WAY */

    #undef a
}
