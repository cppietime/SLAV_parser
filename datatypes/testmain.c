#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "datam.h"

int32_t strindextst(void *ptr, size_t n){
    char *str = (char*)ptr;
    if(n>strlen(str)){
        return 0;
    }
    return str[n];
}

int customcomptst(void *a, void *b){
    char *ca = (char*)a, *cb = (char*)b;
    return strcmp(ca, cb);
}

int maink(){
    srand(time(NULL));
    datam_hashtable *table = datam_hashtable_new(strindextst, customcomptst, 10);
    char *strs[] = {"some", "strings", "might", "work", "but", "maybe", "there", "are", "just", "some", "that", "god", "dang", "dont", "ugh"};
    for(int i = 0; i < 15; i ++){
        datam_hashtable_put(table, strs[i], (void*)rand());
    }
    for(datam_hashbucket *bucket  = datam_hashtable_next(table, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(table, bucket)){
            char *key = bucket->key;
            int r = (int)bucket->value;
            printf("%s: %d\n", key, r);
    }
    datam_hashtable_delete(table);
    return 0;
}