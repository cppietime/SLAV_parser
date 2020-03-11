/*
Implementation of doubly linked list deque.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include "datam.h"

datam_duonode* datam_duonode_new(void *data){
    datam_duonode *ret = malloc(sizeof(datam_duonode));
    ret->next = NULL;
    ret->prev = NULL;
    ret->data = data;
    return ret;
}

datam_deque* datam_deque_new(){
    datam_deque *ret = malloc(sizeof(datam_deque));
    ret->length = 0;
    ret->head = NULL;
    ret->tail = NULL;
    return ret;
}

void datam_deque_delete(datam_deque *deque){
    datam_duonode *node = deque->head, *next;
    while(node != NULL){
        next = node->next;
        free(node);
        node = next;
    }
    free(deque);
}

void datam_deque_insert(datam_deque *deque, void *data, int n){
    datam_duonode *after = datam_deque_get(deque, n);
    datam_duonode *before = NULL;
    if(after != NULL){
        before = after->prev;
    }else{
        before = deque->tail;
    }
    datam_duonode *node = datam_duonode_new(data);
    node->prev = before;
    if(before != NULL){
        before->next = node;
    }else{
        deque->head = node;
    }
    node->next = after;
    if(after != NULL){
        after->prev = node;
    }else{
        deque->tail = node;
    }
    deque->length ++;
}

datam_duonode* datam_deque_remove(datam_deque *deque, int n){
    datam_duonode *node = datam_deque_get(deque, n);
    if(node == NULL){
        return NULL;
    }
    datam_duonode *after = node->next, *before = node->prev;
    node->next = node->prev = NULL;
    if(after != NULL){
        after->prev = before;
    }else{
        deque->tail = before;
    }
    if(before != NULL){
        before->next = after;
    }else{
        deque->head = after;
    }
    deque->length --;
    return node;
}

void datam_deque_set(datam_deque *deque, void *data, int n){
    datam_duonode *node = datam_deque_get(deque, n);
    if(node == NULL){
        return;
    }
    node->data = data;
}

datam_duonode* datam_deque_get(datam_deque *deque, int n){
    if(n<0){
        n += deque->length + 1;
    }
    datam_duonode *node = deque->head;
    for(size_t i=0; i<n && node != NULL; i++){
        node = node->next;
    }
    return node;
}

void datam_deque_queueh(datam_deque *deque, void *data){
    datam_deque_insert(deque, data, 0);
}

datam_duonode* datam_deque_dequeueh(datam_deque *deque){
    return datam_deque_remove(deque, 0);
}

void datam_deque_queuet(datam_deque *deque, void *data){
    datam_deque_insert(deque, data, -1);
}

datam_duonode* datam_deque_dequeuet(datam_deque *deque){
    return datam_deque_remove(deque, -2);
}