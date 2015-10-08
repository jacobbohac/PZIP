#include "config.h"

//---- global params ; must be transmitted @@ ----------

//---- const config -----------------

const uint DETERMINISTIC_MIN_LEN_INC =  2;          /* XXX '1' works a teensy better on the Calgary Corpus, for me. -- CrT */
const uint DETERMINISTIC_MIN_ORDER   = 24;

const int CONTEXT_SYMBOL_INC_NOVEL  = 1;
const int CONTEXT_SYMBOL_INC        = 1;            /* 2 for PPMD , 1 for PPMC */
const int CONTEXT_ESCP_INC          = 1;

const int CONTEXT_ESCAPE_MAX             = 20;    /* Never let escape_count get bigger than this. */
const int CONTEXT_COUNT_HALVE_THRESHOLD  = 4096;  /* Seems to matter very little.  Even order0 doesn't hit this much. */

#ifdef _DEBUG
const int PZIP_PRINTF_INTERVAL = 1000;
#else
const int PZIP_PRINTF_INTERVAL = 10 << 10;
#endif

const uint SEE_INIT_SCALE =  7;
const uint SEE_INIT_ESC   =  8;
const uint SEE_INIT_TOT   = 18;

const uint SEE_INC               =   17;
const uint SEE_ESC_TOT_EXTRA_INC =    1;
const uint SEE_SCALE_DOWN        = 8000;
const uint SEE_ESC_SCALE_DOWN    =  500;

const uint  CONTEXT_EXCLUDED_ESCAPE_SHIFT       = 2;
const uint  CONTEXT_EXCLUDED_ESCAPE_INIT        = 6;
const uint  CONTEXT_EXCLUDED_ESCAPE_INC         = 4;    /* == (1 << CONTEXT_EXCLUDED_ESCAPE_SHIFT) */
const uint  CONTEXT_EXCLUDED_ESCAPE_EXCLUDEDINC = 3;

const uint PZIP_MAX_CONTEXT_LEN   =  32;
const uint PZIP_SEED_BYTES        =   8;
const uint PZIP_SEED_BYTE         = 214;

const uint PZIP_TRIE_MEGS         = 72;
