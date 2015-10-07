#ifndef PZIP_H
#define PZIP_H

#include "inc.h"

uint pzip_Encode(   u08* input_buf,   uint input_len,   u08* comp_buf   );
void pzip_Decode(   u08* input_buf,   uint input_len,   u08* comp_buf   );

#endif /* PZIP_H */

