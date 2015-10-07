#ifndef EXCLUDE_H
#define EXCLUDE_H

struct Excluded_Symbols {
    uint   is_set;
    bool   is_empty;
    uint   symbol_set[ 256 ];
};
typedef struct Excluded_Symbols Excluded_Symbols;

extern Excluded_Symbols* excluded_symbols_Create( void );
extern void     excluded_symbols_Destroy(  Excluded_Symbols* e );
extern void     excluded_symbols_Clear(    Excluded_Symbols* e );
extern void     excluded_symbols_Add(      Excluded_Symbols* e, int sym );
extern bool     excluded_symbols_Contains( Excluded_Symbols* e, int sym );
extern bool     excluded_symbols_Is_Empty( Excluded_Symbols* e );

#ifdef __GNUC__
extern inline bool excluded_symbols_Contains( Excluded_Symbols* e, int sym ) {   return e->symbol_set[ sym ] == e->is_set;   }
#endif

#endif
 
