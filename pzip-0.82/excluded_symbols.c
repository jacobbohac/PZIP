#include "inc.h"
#include "excluded_symbols.h"
#include "safe.h"

/* Excluded_Symbols keeps track of the set of currently excluded symbols */
/* via an explicit 256-long array.                              */
/*                                                              */
/* As a small trick to speed resetting the state to empty,      */
/* we define an slot to be 'set' if it is equal to 'is_set',    */
/* and empty otherwise:  This lets us empty the array in O(1)   */
/* time by incrementing 'is_set' instead of in O(N) time by     */
/* iterating over the array slots in the array.                 */
/*                                                              */
/* The fly in the ointment is that when 'is_set' wraps around   */
/* to zero, we must do an honest O(N) clear of the array.       */
/*                                                              */
/* As a further speed optimization, we use 'is_empty' to tack   */
/* whether the set is currently empty:  This lets us answer     */
/* queries about this in O(1) time instead of O(N) time.        */

struct Excluded_Symbols {
    uint   is_set;
    bool   is_empty;
    uint   symbol_set[ 256 ];
};

bool excluded_symbols_Is_Empty( Excluded_Symbols* e ){   return e->is_empty;   }

static Excluded_Symbols* reset( Excluded_Symbols* e ) {
    memset( e->symbol_set, 0, 256 );
    e->is_set   = 1;
    e->is_empty = TRUE;
    return e;
}

Excluded_Symbols* excluded_symbols_Create( void )              {   return reset( new( Excluded_Symbols ) );            }
void excluded_symbols_Destroy( Excluded_Symbols* e )           {   destroy( e );                              }
bool excluded_symbols_Contains( Excluded_Symbols* e, int sym ) {   return e->symbol_set[ sym ] == e->is_set;   }

void excluded_symbols_Clear( Excluded_Symbols* e ) {
    e->is_empty = TRUE;
    if (! ++e->is_set)   reset( e );
}

void excluded_symbols_Add(   Excluded_Symbols* e,   int sym   ) {
    e->symbol_set[ sym ] = e->is_set;
    e->is_empty          = FALSE;
}

 
