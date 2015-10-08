#ifndef PZIP_H
#define PZIP_H

#include "inc.h"

uint pzip_Encode(   ubyte* input_buf,   uint input_len,   ubyte* comp_buf   );
void pzip_Decode(   ubyte* input_buf,   uint input_len,   ubyte* comp_buf   );

#endif /* PZIP_H */

