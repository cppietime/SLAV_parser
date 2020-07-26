/*
Implementation of dynamic arrays.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "datam.h"

#define INITIAL_DARR_CAP 8

datam_darr* datam_darr_new(size_t size){
    datam_darr *ret = malloc(sizeof(datam_darr));
    ret->size = size;
    ret->n = 0;
    ret->c = 8;
    ret->data = malloc(size*ret->c);
    return ret;
}

void datam_darr_delete(datam_darr *list){
    free(list->data);
    free(list);
}

void datam_darr_push(datam_darr *list, void *src){
    if(list->n == list->c){
        list->c <<= 1;
        list->data = realloc(list->data, list->size * list->c);
    }
    memcpy(list->data + list->n * list->size, src, list->size);
    list->n ++;
}

void datam_darr_pushlit(datam_darr *list, long val){
	if(list->n == list->c){
		list->c <<= 1;
		list->data = realloc(list->data, list->size * list->c);
	}
	switch(list->size){
		case 1:{
			uint8_t *data = list->data;
			data[list->n] = val;
			break;
		}
		case 2:{
			uint16_t *data = list->data;
			data[list->n] = val;
			break;
		}
		case 4:{
			uint32_t *data = list->data;
			data[list->n] = val;
			break;
		}
	}
	list->n++;
}

void datam_darr_pushall(datam_darr *dst, datam_darr *src){
	static unsigned char buff[2048];
	for(size_t i = 0; i < src->n; i++){
		datam_darr_get(src, buff, i);
		datam_darr_push(dst, buff);
	}
}

int datam_darr_pop(datam_darr *list, void *dst){
    if(list->n == 0){
        return 0;
    }
    list->n --;
    if(dst != NULL){
        memcpy(dst, list->data + list->n * list->size, list->size);
    }
    return 1;
}

int datam_darr_set(datam_darr *list, void *src, size_t n){
    if(n >= list->n){
        return 0;
    }
    memcpy(list->data + n * list->size, src, list->size);
    return 1;
}

int datam_darr_get(datam_darr *list, void *dst, size_t n){
    if(n >= list->n){
        return 0;
    }
    memcpy(dst, list->data + n * list->size, list->size);
    return 1;
}

int datam_darr_insert(datam_darr *list, void *src, size_t n){
    if(n == list->n){
        datam_darr_push(list, src);
        return 1;
    }
    if(n > list->n){
        return 0;
    }
    if(list->n == list->c){
        list->c <<= 1;
        list->data = realloc(list->data, list->size * list->c);
    }
    memmove(list->data + (n + 1) * list->size, list->data + n * list->size, (list->n - n) * list->size);
    memcpy(list->data + n * list->size, src, list->size);
    list->n ++;
    return 1;
}

int datam_darr_remove(datam_darr *list, void *dst, size_t n){
    if(n == list->n){
        return datam_darr_pop(list, dst);
    }
    if(n > list->n){
        return 0;
    }
    if(dst != NULL){
        memcpy(dst, list->data + n * list->size, list->size);
    }
    list->n --;
    memmove(list->data + n * list->size, list->data + (n + 1) * list->size, (list->n - n) * list->size);
    return 1;
}

int datam_darr_cmp(void *va, void *vb){
    datam_darr *a = (datam_darr*)va, *b = (datam_darr*)vb;
    if(a->size != b->size){
        return (int)a->size - (int)b->size;
    }
    if(a->n != b->n){
        return (int)a->n - (int)b->n;
    }
    return memcmp(a->data, b->data, a->n*a->size);
}

int32_t datam_darr_indexer(void *a, size_t n){
    datam_darr *list = (datam_darr*)a;
    int32_t ret = 0;
    char *cptr = (char*)list->data;
    for(size_t i = 0; i < list->n * list->size; i += n + 1){
        ret += (i + 1) * cptr[i];
    }
    return ret;
}

void datam_darr_sort(datam_darr *list, int (*compar)(const void *a, const void *b)){
    qsort(list->data, list->n, list->size, compar);
}

#undef INITIAL_DARR_CAP