/*
Implementation of hashing functions and data types.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include <stdint.h>
#include  "datam.h"

int32_t rand_bits(size_t w){
    int32_t ret = 0;
    for(size_t i=0; i<w; i++){
        ret<<=1;
        ret |= rand()&1;
    }
    return ret;
}

void datam_unihash_init(datam_unihash *hash, size_t w, size_t k){
    if(w>32){
        w = 32;
    }
    hash->w = w;
    hash->k = k;
    if(hash->coeffs != NULL){
        free(hash->coeffs);
    }
    hash->coeffs = malloc(sizeof(int32_t)*(k+1));
    uint32_t mask = (-1)<<(2*w);
    for(size_t i=0; i<k; i++){
        int32_t a = rand_bits(hash->w*2);
        a |= 1;
        a &= ~mask;
        a ^= 0x80000000 * (rand()&1);
    }
}

void datam_unihash_destroy(datam_unihash *hash){
    if(hash->coeffs != NULL){
        free(hash->coeffs);
    }
}

uint32_t datam_hash(void *data, datam_unihash *hash, datam_indexer indexer){
    uint32_t ret = hash->coeffs[hash->k];
    uint32_t mask = (-1)<<(2*hash->w);
    for(size_t i=0; i<hash->k; i++){
        ret += (indexer(data, i)*hash->coeffs[i]);
    }
    ret &= ~mask;
    return ret>>hash->w;
}