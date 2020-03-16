/*
Implementation of the open addressed hash table.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "datam.h"

static void* _EMPTY;

#define START_CAPACITY_L2 3

datam_hashtable* datam_hashtable_new(datam_indexer indexer, datam_comparator comp, size_t k){
    datam_hashtable *ret = malloc(sizeof(datam_hashtable));
    ret->indexer = indexer;
    ret->comp = comp;
    ret->hash.coeffs = NULL;
    ret->w = START_CAPACITY_L2;
    ret->c = 1 << ret->w;
    ret->k = k;
    ret->n = 0;
    datam_unihash_init(&ret->hash, ret->w, k);
    ret->buckets = malloc(sizeof(datam_hashbucket)*ret->c);
    for(size_t i=0; i<ret->c; i++){
        datam_hashbucket* bucket = ret->buckets+i;
        *bucket = (datam_hashbucket){.key = NULL, .value = NULL, .next = 0};
    }
    return ret;
}

void datam_hashtable_delete(datam_hashtable *table){
    free(table->buckets);
    datam_unihash_destroy(&table->hash);
    free(table);
}

int datam_hashtable_put(datam_hashtable *table, void *key, void *value){
    datam_hashbucket *present = datam_hashtable_has(table, key);
    if(present != NULL){
        present->value = value;
        return 0;
    }
    if(table->n == table->c){ /* Resize the hash table to double its current capacity; this requires reallocating the buckets, making a new hash, and inserting the old
                                key value pairs to their respective new positions for that new hash. */
        datam_hashbucket *tmp = table->buckets;
        table->w ++;
        table->c = 1<<table->w;
        size_t oldn = table->n;
        table->n = 0;
        datam_unihash_init(&table->hash, table->w, table->k);
        table->buckets = malloc(sizeof(datam_hashbucket)*table->c);
        for(size_t i=0; i<table->c; i++){
            datam_hashbucket* bucket = table->buckets+i;
            *bucket = (datam_hashbucket){.key = NULL, .value = NULL, .next = 0};
        }
        for(size_t i=0; i<oldn; i++){
            if(tmp[i].key != NULL && tmp[i].key != ( NULL )){
                datam_hashtable_put(table, tmp[i].key, tmp[i].value);
            }
        }
        free(tmp);
    }
    uint32_t hash = datam_hash(key, &table->hash, table->indexer) % table->c;
    uint32_t bucket = hash;
    while(table->buckets[bucket].key != NULL){
        if(table->buckets[bucket].hash == hash && !table->comp(table->buckets[bucket].key, key)){
            break; /* If the key is already present in this table, we simply replace it with the new one */
        }
        if(table->buckets[bucket].next == 0){
            table->buckets[bucket].next = 1;
        }
        bucket += 1;
        bucket %= table->c;
    }
    datam_hashbucket *hashbucket = table->buckets+bucket;
    if(hashbucket->key == NULL || hashbucket->key == ( NULL )){
        table->n++;
    }
    hashbucket->hash = hash;
    hashbucket->key = key;
    hashbucket->value = value;
    return 1;
}

void* datam_hashtable_get(datam_hashtable *table, void *key){
    datam_hashbucket *bucket = datam_hashtable_has(table, key);
    if(bucket == NULL){
        return NULL;
    }
    return bucket->value;
}

datam_hashbucket* datam_hashtable_has(datam_hashtable *table, void *key){
    uint32_t hash = datam_hash(key, &table->hash, table->indexer) % table->c;
    uint32_t bucket = hash;
    while(table->buckets[bucket].key != NULL){
        if(table->buckets[bucket].hash == hash && !table->comp(table->buckets[bucket].key, key)){
            return table->buckets+bucket;
        }
        if(table->buckets[bucket].next == 0){
            return NULL;
        }
        bucket += 1;
        bucket %= table->c;
        if(bucket == hash){
            break;
        }
    }
    return NULL;
}

void* datam_hashtable_remove(datam_hashtable *table, void *key){
    datam_hashbucket *bucket = datam_hashtable_has(table, key);
    if(bucket == NULL){
        return NULL;
    }
    void* toret  = bucket->value;
    bucket->value = NULL;
    bucket->key = NULL;
    table->n--;
    return toret;
}

datam_hashbucket* datam_hashtable_next(datam_hashtable *table, datam_hashbucket *bucket){
    if(table->n == 0){
        return NULL;
    }
    size_t start;
    if(bucket == NULL){
        start = 0;
    }else{
        start = bucket - table->buckets + 1;
    }
    while(start < table->c){
        datam_hashbucket *candidate = table->buckets+start;
        if(candidate->key != NULL){
            return candidate;
        }
        start ++;
    }
    return NULL;
}

void datam_hashtable_clear(datam_hashtable *table){
    for(size_t i = 0; i < table->c; i ++){
        datam_hashbucket *bucket = table->buckets + i;
        bucket->key = NULL;
        bucket->value = NULL;
        bucket->next = 0;
        bucket->hash = 0;
    }
    table->n = 0;
}

void datam_hashtable_addall(datam_hashtable *dst, datam_hashtable *src){
    for(datam_hashbucket *bucket = datam_hashtable_next(src, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(src, bucket)){
            if(datam_hashtable_has(dst, bucket->key)==NULL){
                datam_hashtable_put(dst, bucket->key, bucket->value);
            }
    }
}

int32_t datam_hashtable_ptrset_indexer(void *data, size_t n){
    datam_hashtable *table = (datam_hashtable*)data;
    if(n > 0){
        return table->n * n;
    }
    int32_t ret = 0;
    for(datam_hashbucket *bucket = datam_hashtable_next(table, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(table, bucket)){
            ret += (int32_t)(bucket->key);
    }
    return ret;
}

int datam_hashtable_ptrset_comp(void *a, void *b){
    datam_hashtable *tablea = (datam_hashtable*)a;
    datam_hashtable *tableb = (datam_hashtable*)b;
    for(datam_hashbucket *bucket = datam_hashtable_next(tablea, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(tablea, bucket)){
            if(datam_hashtable_has(tableb, bucket->key)==NULL){
                return 1;
            }
    }
    for(datam_hashbucket *bucket = datam_hashtable_next(tableb, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(tableb, bucket)){
            if(datam_hashtable_has(tablea, bucket->key)==NULL){
                return -1;
            }
    }
    return 0;
}

int32_t datam_hashtable_i_indexer(void *data, size_t n){
    return (int32_t)data;
}

int datam_hashtable_i_cmp(void *a, void *b){
    int i = (int)a, j = (int)b;
    return i - j;
}

#undef START_CAPACITY_L2