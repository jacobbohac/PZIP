#ifndef INC_H
#define INC_H

typedef unsigned short uword;
typedef unsigned char  ubyte;
typedef short word;
typedef ubyte byte;
typedef int   bool;

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "safe.h"

#ifndef ULONG_MAX
#define ULONG_MAX (~((ulong)0))  /* Max value for unsigned long int  */
#endif

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef ABS
#define ABS(i) ((i) < 0 ? -(i) : (i))
#endif

#define getulong(bptr) ( ((((ubyte *)(bptr))[0])<<24) + (((ubyte *)(bptr))[1]<<16) + (((ubyte *)(bptr))[2]<<8) + (((ubyte *)(bptr))[3]) )
#define new(dtype) safe_Calloc( 1, sizeof(dtype) )
#define destroy(m) if ( (m) == NULL ) ; else { free((void *)m); m = NULL; }

extern int verbose;

#endif /* INC_H */

