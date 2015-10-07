#ifndef INC_H
#define INC_H

#ifdef PRODUCTION
#define NDEBUG  /* Don't compile asserts(). */
#endif

#ifdef __GNUC__
#define inline __inline__
#else
#define inline
#endif

typedef unsigned long long   u64;
typedef   signed long long   i64;

typedef unsigned int   u32;
typedef   signed int   i32;

typedef unsigned short u16;
typedef signed short   i16;

typedef unsigned char  u08;
typedef   signed char  i08;

typedef u08 byte;
typedef int bool;

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "safe.h"

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

#define getu32(bptr) ( ((((u08 *)(bptr))[0])<<24) + (((u08 *)(bptr))[1]<<16) + (((u08 *)(bptr))[2]<<8) + (((u08 *)(bptr))[3]) )
#define new(dtype) safe_Calloc( 1, sizeof(dtype) )
#define destroy(m) if ( (m) == NULL ) ; else { free((void *)m); m = NULL; }

extern int verbose;

#endif /* INC_H */

