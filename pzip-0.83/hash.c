#include <stdio.h>
#include "inc.h"
#include "hash.h"

Context* hashtab_02[ HASH_SLOTS_02 ];
Context* hashtab_03[ HASH_SLOTS_03 ];
Context* hashtab_04[ HASH_SLOTS_04 ];
Context* hashtab_05[ HASH_SLOTS_05 ];
Context* hashtab_08[ HASH_SLOTS_08 ];
Context* hashtab_12[ HASH_SLOTS_12 ];
Context* hashtab_16[ HASH_SLOTS_16 ];

void     hash_Note_Context_02(   Context* context,   Suffix suffix   ) {
    hashtab_02[ suffix._0_to_7.u_16 ] = context;
}

void     hash_Drop_Context_02(   Context* context   ) {
    hashtab_02[ context->suffix._0_to_7.u_16 ] = NULL;
}

Context* hash_Find_Context_02(   Suffix suffix   ) {
    return hashtab_02[ suffix._0_to_7.u_16 ];
}




void     hash_Note_Context_03(   Context* context,   Suffix suffix   ) {

    u32 hash32 = suffix._0_to_7.u_32;
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_03;

    context->hashlink = hashtab_03[ hash32 ];
    hashtab_03[ hash32 ] = context;
}

void     hash_Drop_Context_03(   Context* context   ) {

    Suffix suffix = context->suffix;
    u32 hash32 = suffix._0_to_7.u_32;
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_03;

    {   Context** patchpoint = &hashtab_03[ hash32 ];
        Context*  c;
        for (c = *patchpoint;   c;   patchpoint = &c->hashlink, c = *patchpoint) {
            if (c->suffix._0_to_7.u_32 == suffix._0_to_7.u_32 ){
                *patchpoint = c->hashlink;
                return;  
            }
        }
    }
    assert( 0 && "Attempt to drop Context not in hashtab_03[]" );
}

Context* hash_Find_Context_03(   Suffix suffix   ) {

    u32 hash32 = suffix._0_to_7.u_32;
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_03;

    {   Context* c;
        for (c = hashtab_03[ hash32 ];   c;   c = c->hashlink) {
            if (c->suffix._0_to_7.u_32 == suffix._0_to_7.u_32){
                return c;
            }
        }
    }
    return NULL;
}




void     hash_Note_Context_04(   Context* context,   Suffix suffix   ) {

    u32 hash32 = suffix._0_to_7.u_32;
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_04;

    context->hashlink = hashtab_04[ hash32 ];
    hashtab_04[ hash32 ] = context;
}

void     hash_Drop_Context_04(   Context* context   ) {

    Suffix suffix = context->suffix;

    u32 hash32 = suffix._0_to_7.u_32;
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_04;

    {   Context** patchpoint = &hashtab_04[ hash32 ];
        Context*  c;
        for (c = *patchpoint;   c;   patchpoint = &c->hashlink, c = *patchpoint) {
            if (c->suffix._0_to_7.u_32 == suffix._0_to_7.u_32){
                *patchpoint = c->hashlink;
                return;  
            }
        }
    }
    assert( 0 && "Attempt to drop Context not in hashtab_04[]" );
}

Context* hash_Find_Context_04(   Suffix suffix   ) {

    u32 hash32 = suffix._0_to_7.u_32;
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_04;

    {   Context* c;
        for (c = hashtab_04[ hash32 ];   c;   c = c->hashlink) {
            if (c->suffix._0_to_7.u_32 == suffix._0_to_7.u_32){
                return c;
            }
        }
    }
    return NULL;
}




void     hash_Note_Context_05(   Context* context,   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_05;

    context->hashlink = hashtab_05[ hash32 ];
    hashtab_05[ hash32 ] = context;
}

void     hash_Drop_Context_05(   Context* context   ) {

    Suffix suffix = context->suffix;

    u64 hash64 = suffix._0_to_7.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_05;

    {   Context** patchpoint = &hashtab_05[ hash32 ];
        Context*  c;
        for (c = *patchpoint;   c;   patchpoint = &c->hashlink, c = *patchpoint) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64){
                *patchpoint = c->hashlink;
                return;  
            }
        }
    }
    assert( 0 && "Attempt to drop Context not in hashtab_5[]" );
}

Context* hash_Find_Context_05(   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_05;

    {   Context* c;
        for (c = hashtab_05[ hash32 ];   c;   c = c->hashlink) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64){
                return c;
            }
        }
    }
    return NULL;
}




void     hash_Note_Context_08(   Context* context,   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_08;

    context->hashlink = hashtab_08[ hash32 ];
    hashtab_08[ hash32 ] = context;
}

void     hash_Drop_Context_08(   Context* context   ) {

    Suffix suffix = context->suffix;

    u64 hash64 = suffix._0_to_7.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_08;

    {   Context** patchpoint = &hashtab_08[ hash32 ];
        Context*  c;
        for (c = *patchpoint;   c;   patchpoint = &c->hashlink, c = *patchpoint) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64){
                *patchpoint = c->hashlink;
                return;  
            }
        }
    }
    assert( 0 && "Attempt to drop Context not in hashtab_08[]" );
}

Context* hash_Find_Context_08(   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_08;

    {   Context* c;
        for (c = hashtab_08[ hash32 ];   c;   c = c->hashlink) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64){
                return c;
            }
        }
    }
    return NULL;
}







void     hash_Note_Context_12(   Context* context,   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64 + suffix._8_to_F.u_32;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_12;

    context->hashlink = hashtab_12[ hash32 ];
    hashtab_12[ hash32 ] = context;
}

void     hash_Drop_Context_12(   Context* context   ) {

    Suffix suffix = context->suffix;

    u64 hash64 = suffix._0_to_7.u_64 + suffix._8_to_F.u_32;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_12;

    {   Context** patchpoint = &hashtab_12[ hash32 ];
        Context*  c;
        for (c = *patchpoint;   c;   patchpoint = &c->hashlink, c = *patchpoint) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64
            &&  c->suffix._8_to_F.u_32 == suffix._8_to_F.u_32
            ){
                *patchpoint = c->hashlink;
                return;  
            }
        }
    }
    assert( 0 && "Attempt to drop Context not in hashtab_12[]" );
}

Context* hash_Find_Context_12(   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64 + suffix._8_to_F.u_32;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_12;

    {   Context* c;
        for (c = hashtab_12[ hash32 ];   c;   c = c->hashlink) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64
            &&  c->suffix._8_to_F.u_32 == suffix._8_to_F.u_32
            ){
                return c;
            }
        }
    }
    return NULL;
}




void     hash_Note_Context_16(   Context* context,   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64 + suffix._8_to_F.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_16;

    context->hashlink = hashtab_16[ hash32 ];
    hashtab_16[ hash32 ] = context;
}

void     hash_Drop_Context_16(   Context* context   ) {

    Suffix suffix = context->suffix;

    u64 hash64 = suffix._0_to_7.u_64 + suffix._8_to_F.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_16;

    {   Context** patchpoint = &hashtab_16[ hash32 ];
        Context*  c;
        for (c = *patchpoint;   c;   patchpoint = &c->hashlink, c = *patchpoint) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64
            &&  c->suffix._8_to_F.u_64 == suffix._8_to_F.u_64
            ){
                *patchpoint = c->hashlink;
                return;  
            }
        }
    }
    assert( 0 && "Attempt to drop Context not in hashtab_16[]" );
}

Context* hash_Find_Context_16(   Suffix suffix   ) {

    u64 hash64 = suffix._0_to_7.u_64 + suffix._8_to_F.u_64;
    u32 hash32 = hash64 + (hash64 >> 32);
    hash32     = hash32 + (hash32 >> 16);
    hash32    &= HASH_MASK_16;

    {   Context* c;
        for (c = hashtab_16[ hash32 ];   c;   c = c->hashlink) {
            if (c->suffix._0_to_7.u_64 == suffix._0_to_7.u_64
            &&  c->suffix._8_to_F.u_64 == suffix._8_to_F.u_64
            ){
                return c;
            }
        }
    }
    return NULL;
}

