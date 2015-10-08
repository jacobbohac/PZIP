#ifndef POOL_H
#define POOL_H

#include "inc.h"

typedef struct Pool Pool;

void* pool_Auto_Get_Hunk(  Pool** pool, int* hunk_count, int hunk_size );
void  pool_Auto_Free_Hunk( Pool** pool, int* hunk_count, void* hunk   );
void  pool_Auto_Destroy(  Pool** pool, int* hunk_count               );

extern Pool* pool_Create( long hunk_length, long hunk_count, long num_auto_extend_items );
extern void  pool_Destroy(  Pool* pool ); /* ok to call this with pool == NULL */
extern void* pool_Get_Hunk( Pool* pool );

#endif /* POOL_H */
