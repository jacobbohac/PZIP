#include "inc.h"
#include "pool.h"
#include "safe.h"

typedef struct Block {
    struct Block* next;
    char*         base;
    char*         ptr;
    long          length;
    long          free;
} Block;

struct Pool {
    long   hunk_length;

    Block* this_block;
    Block* block;
    void** freed_hunks;

    long   num_auto_extend_items;
    long   freed_hunk_count;
    long   freed_hunk_count_max;
    long   active_item_count;
};

/*
 *  Pool
 *    system for fast Resetting & Freeing of nodes; for use in tree structures
 *     requires no traversing for freeing
 *    does auto-extending of memory space in case you use more space than
 *     expected or if you don't know how much you will need
 *
 */

static Pool* extend( Pool* pool, long hunk_count ) {
    Block* block = pool->block;

    while (block) {
        if (block->free > pool->hunk_length) {
            pool->this_block = block;
            return pool;
        }
        block = block->next;
    }

    block = safe_Malloc( sizeof( Block ) );
    block->length = hunk_count * pool->hunk_length;
    block->free   = block->length;

    block->base = safe_Malloc( block->length );
    block->ptr  = block->base;
    block->next = pool->block;
    pool->block = block;

    pool->this_block = block;

    return pool;
}

#define padded_size(a) ((((a)-1)/4 + 1)*4)


/*
 * hunk_length is forced to be a multiple of 4
 */
Pool* pool_Create( long hunk_length, long NumHunks, long num_auto_extend_items ) {

    Pool* pool = safe_Malloc( sizeof( Pool ));
    pool->hunk_length  = padded_size( hunk_length );
    pool->this_block   = NULL;
    pool->block        = NULL;

    pool->freed_hunk_count      = 0;
    pool->freed_hunk_count_max  = 16;
    pool->num_auto_extend_items = num_auto_extend_items;
    pool->active_item_count     = 0;

    pool->freed_hunks = safe_Malloc( pool->freed_hunk_count_max * sizeof(void*) );

    return extend( pool, NumHunks );
}

void pool_Destroy( Pool* pool ) {
    Block* this;
    Block* next;

    if (pool == NULL)  return;

    for (this = pool->block;  this;   this = next) {
        free( this->base );
        next = this->next;
        free( this );
    }

    free( pool->freed_hunks );
    free( pool               );
}

void* pool_Get_Hunk( Pool* pool ) {
    void*  ret;
    Block* block;

    if (pool->freed_hunk_count > 0) {
        --pool->freed_hunk_count;
        ret = pool->freed_hunks[ pool->freed_hunk_count ];

        pool->active_item_count++;

        memset( ret, 0, pool->hunk_length );

        return ret;
    }

    if (!(block = pool->this_block)) {
        return NULL;
    }

    if (block->free < pool->hunk_length) {
        pool = extend( pool, pool->num_auto_extend_items );
        block = pool->this_block;
    }

    ret = (void*) block->ptr;

    block->free -= pool->hunk_length;
    block->ptr  += pool->hunk_length;

    memset( ret, 0, pool->hunk_length );

    ++ pool->active_item_count;

    return ret;
}

static bool free_hunk( Pool* pool, void* hunk ) {

    if ( pool->freed_hunk_count >= pool->freed_hunk_count_max ) {
        void** newFreedHunks = safe_Malloc( (pool->freed_hunk_count_max << 1) * sizeof(void*) );
        memcpy( newFreedHunks, pool->freed_hunks, pool->freed_hunk_count_max * sizeof(void*) );
        free( pool->freed_hunks );
        pool->freed_hunks = newFreedHunks;
        pool->freed_hunk_count_max <<= 1;
    }

    pool->freed_hunks[ pool->freed_hunk_count ] = hunk;

    ++pool->freed_hunk_count;
    --pool->active_item_count;

    return 1;
}

void* pool_Auto_Get_Hunk( Pool** pool,  int* hunk_count,   int hunk_size ) {
    void* hunk;

    if (!*pool) {
        *hunk_count = 0;
        *pool = pool_Create( hunk_size, 100 * 4096, 100 * 1024 );    assert( *pool );
    }

    hunk = pool_Get_Hunk( *pool );

    *hunk_count += 1;

    return hunk;
}

void  pool_Auto_Free_Hunk( Pool** pool, int* hunk_count, void* hunk ) {
    assert( *pool && *hunk_count >= 1 );

    free_hunk( *pool, hunk );

    *hunk_count -= 1;

    if (*hunk_count == 0) {
        pool_Destroy( *pool );
        *pool = NULL;
    }
}

void pool_Auto_Destroy( Pool** pool,int* hunk_count ) {
    if (*pool) {
        pool_Destroy( *pool );
    }
    *pool = NULL;
    *hunk_count = 0;
}
