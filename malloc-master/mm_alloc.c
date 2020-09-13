/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void * startOfMem = (void *)-1;

typedef struct metadata {
    void * prev, * next;
    int free;
    unsigned int size;
} metadata;

void *mm_malloc(size_t size) {
    if(size == 0){
        return NULL;
    }
    if(startOfMem == (void *)-1){
        void * addr = sbrk(size + sizeof(metadata));
        if(addr == (void *)-1){
            return NULL;
        }
        startOfMem = addr;
        metadata * curMet = (metadata *) addr;
        curMet->prev = NULL;
        curMet->next = NULL;
        curMet->free = 0;
        curMet->size = size;
        memset(addr + sizeof(metadata), 0, size);
        return addr + sizeof(metadata);
    }
    metadata * lastMet = NULL;
    metadata * curMet = startOfMem;
    while(curMet != NULL){
        if(curMet->free && curMet->next == NULL){
            if(curMet->size < size){
                void * addedAddr = sbrk(size - curMet->size);
                if(addedAddr == (void *)-1) {
                    return NULL;
                }
            } else {
                if(curMet->size >= size + sizeof(metadata)){
                    metadata * nextOfCur = (void *)curMet + sizeof(metadata) + size;
                    curMet->next = nextOfCur;
                    nextOfCur->prev = curMet;
                    nextOfCur->size = curMet->size - size - sizeof(metadata);
                    nextOfCur->next = NULL;
                    nextOfCur->free = 1;
                }
            }
            curMet->size = size;
            curMet->free = 0;
            memset((void*)curMet + sizeof(metadata), 0, size);
            return (void *)curMet + sizeof(metadata);
        }
        if(curMet->free && curMet->size >= size){
            curMet->free = 0;
            curMet->size = size;
            memset((void*)curMet + sizeof(metadata), 0, size);
            if(curMet->next - (void*)curMet - sizeof(metadata) >= size + sizeof(metadata)){ // curMet->size is already changed
                metadata * addedMeta = (void * )curMet + sizeof(metadata) + size;
                metadata * nextOfCur = curMet->next;
                curMet->next = addedMeta;
                addedMeta->prev = curMet;
                addedMeta->next = nextOfCur;
                if(nextOfCur != NULL){
                    nextOfCur->prev = addedMeta;
                }
                addedMeta->free = 1;
                addedMeta->size = addedMeta->next - (void * )addedMeta - sizeof(metadata);
            }
            return (void*)curMet + sizeof(metadata);
        }
        lastMet = curMet;
        curMet = curMet->next;
    }
    void * addedAddr = sbrk(size + sizeof(metadata));
    //printf("came here  ffffffffff\n");
    if(addedAddr == (void *)-1) {
        return NULL;
    }
    metadata * addedMet = (metadata *)addedAddr;
    addedMet->prev = lastMet;
    lastMet->next = addedMet;
    addedMet->next = NULL;
    addedMet->free = 0;
    addedMet->size = size;
    memset((void*)addedMet + sizeof(metadata), 0, size);
    //printf("came here \n");
    return (void*)addedMet + sizeof(metadata);
}

void *mm_realloc(void *ptr, size_t size) {
    if(size == 0){
        mm_free(ptr);
        return NULL;
    }
    if(ptr == NULL){
        return mm_malloc(size);
    }
    size_t oldSize = ((metadata *)(ptr-sizeof(metadata)))->size;
    mm_free(ptr);
    void * dest = mm_malloc(size);
    if(dest) {
        return memcpy(dest, ptr, oldSize);
    }
    // metadata * curMet = startOfMem;
    // size_t minSize = __SIZE_MAX__;
    // metadata * minPtr = NULL;
    // metadata * lastPtr = NULL;
    // while(curMet){
    //     if(curMet->free && curMet->size >= size) {
    //         if(curMet->size < minSize){
    //             minSize = curMet->size;
    //             minPtr = curMet;
    //         }
    //     }
    //     lastPtr = curMet;
    //     curMet = curMet->next;
    // }
    // if(minPtr){
    //     if(minPtr->size >= size + sizeof(metadata)){
    //         metadata * nextMet = (void *)minPtr + size + sizeof(metadata);
    //         nextMet->next = minPtr->next;
    //         ((metadata *)nextMet->next)->prev = nextMet;
    //         nextMet->free = 1;
    //         minPtr->next = nextMet;
    //         nextMet->prev = minPtr;
    //         nextMet->size = nextMet->next - (void *)nextMet - sizeof(metadata);

    //     }
    //     minPtr->size = size;
    //     minPtr->free = 0;
    //     return memcpy((void *)minPtr + sizeof(metadata), ptr, size);
    // }
    // if(lastPtr && lastPtr->free){
    //     sbrk(size - lastPtr->size);

    // }
    return NULL;
}

void mm_free(void *ptr) {
    if(ptr == NULL) return;

    metadata * deletingAddr = ptr - sizeof(metadata);
    if(deletingAddr->prev != NULL && ((metadata *)deletingAddr->prev)->free) {
        ((metadata *)deletingAddr->prev)->size += (deletingAddr->size + sizeof(metadata));
        ((metadata *)deletingAddr->prev)->next = deletingAddr->next;
        deletingAddr = (metadata *)deletingAddr->prev;
    }
    if(deletingAddr->next != NULL && ((metadata *)deletingAddr->next)->free) {
        metadata * justNext = deletingAddr->next;
        deletingAddr->size += (justNext->size + sizeof(metadata));
        metadata * nextOfNext = (justNext)->next;
        deletingAddr->next = nextOfNext;
        if(nextOfNext != NULL){
            nextOfNext->prev = deletingAddr;
        }
    }
    deletingAddr->free = 1;
}
