#ifndef CONFIG_H
#define CONFIG_H

#include "inc.h"

#define PZIP_ORDER (8)                  /* The number of Trie indices. */

#define PZIP_INTPROB_SHIFT      (16)    /* Making this small hurts compression !! */
#define PZIP_INTPROB_ONE        (1UL << PZIP_INTPROB_SHIFT)

extern const uint DETERMINISTIC_MIN_LEN_INC;
extern const uint DETERMINISTIC_MIN_ORDER  ;

extern const int CONTEXT_SYMBOL_INC_NOVEL;
extern const int CONTEXT_SYMBOL_INC      ;
extern const int CONTEXT_ESCP_INC     ;

extern const int CONTEXT_ESCAPE_MAX           ;
extern const int CONTEXT_COUNT_HALVE_THRESHOLD;

extern const int PZIP_PRINTF_INTERVAL;

extern const uint SEE_INIT_SCALE;
extern const uint SEE_INIT_ESC  ;
extern const uint SEE_INIT_TOT  ;

extern const uint SEE_INC              ;
extern const uint SEE_ESC_TOT_EXTRA_INC;
extern const uint SEE_SCALE_DOWN       ;
extern const uint SEE_ESC_SCALE_DOWN   ;

extern const uint  CONTEXT_EXCLUDED_ESCAPE_SHIFT      ;
extern const uint  CONTEXT_EXCLUDED_ESCAPE_INIT       ;
extern const uint  CONTEXT_EXCLUDED_ESCAPE_INC        ;
extern const uint  CONTEXT_EXCLUDED_ESCAPE_EXCLUDEDINC;

extern const uint PZIP_MAX_CONTEXT_LEN;
extern const uint PZIP_SEED_BYTES     ;
extern const uint PZIP_TRIE_MEGS      ;
extern const uint PZIP_SEED_BYTE      ;

#endif /* CONFIG_H */
