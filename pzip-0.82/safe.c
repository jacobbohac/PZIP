#include <stdio.h>
#include <stdlib.h>

/* Trivial wrappers to save us from having to */
/* litter the code with tests for NULL every- */
/* where we allocate memory:                  */

void* safe_Malloc( size_t size ) {
    void* result = malloc( size );
    if (result)   return result;
    fputs( "Out of memory!", stderr );
    exit(1);
}

void* safe_Calloc( size_t nmemb, size_t size ) {
    void* result = calloc( nmemb, size );
    if (result)   return result;
    fputs( "Out of memory!", stderr );
    exit(1);
}


    
