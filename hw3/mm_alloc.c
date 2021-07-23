/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/**
 * size -32
*/
// typedef struct{
//   struct block * prev;
//   struct block * next; 
//   bool free;
//   size_t size;
// }block;

static struct block head;
static struct block tail;

void *mm_malloc(size_t size) {
    if(size == 0) return NULL;
    struct block * aquaire_block = (struct block *) find_ff(size);
    if(aquaire_block == NULL){
        aquaire_block = add_to_list(size);
        if(aquaire_block == NULL) return NULL;
    }
    memset((char*)aquaire_block+sizeof(struct block),0,aquaire_block->size);
    return (char*)aquaire_block+sizeof(struct block);
}

void *freeLastAllocNew(void* ptr, size_t size){
    struct block * data = (struct block * )((char*)ptr - sizeof(struct block));  
    char info[data->size];
    memcpy(info,ptr,data->size);
    mm_free(ptr);
    void* newblock = mm_malloc(size);
    memcpy(newblock,info,size);
    return newblock;
}
void *mm_realloc(void *ptr, size_t size) {
    if(ptr == NULL) return mm_malloc(size);
    if(size == 0){
        mm_free(ptr);
        return NULL;
    }
    return freeLastAllocNew(ptr,size);
}
void * add_to_list(size_t size){
    struct block *newBl = sbrk(size + sizeof(struct block));
    if(newBl == (struct block*)-1) return NULL;
    newBl->free = false;
    newBl->size = size;
    if(tail.prev != NULL){
        tail.prev->next = newBl;
        newBl->prev=tail.prev;
        tail.prev=newBl;
        newBl->next = &tail;    
    }else{
        newBl->prev=&head;
        head.next=newBl;
        tail.prev = newBl;
        newBl->next = &tail;    
    }
    return newBl;
}


void mm_free(void *ptr) {
    if(ptr == NULL) return;
    struct block *currBlock = (struct block* )((char*)ptr - sizeof(struct block));
    if(currBlock->free == true) return;
    if(currBlock->next != &tail && currBlock->next->free == true){
        currBlock->next = currBlock->next->next;
        currBlock->next->next->prev=currBlock;
        currBlock->size += currBlock->next->size +sizeof(struct block);
    }
    if(currBlock->prev != &head && currBlock->prev->free == true){
        currBlock->prev->next = currBlock->next;
        currBlock->next->prev=currBlock->prev;
        currBlock->prev->size += currBlock->size + sizeof(struct block);

       currBlock = currBlock->prev;
    }
    currBlock->free = true;
}



void * find_ff(size_t size){
    if(head.next == NULL) return NULL;
    struct block * curr = head.next;
    while((curr != &tail) && (curr->free == false || curr->size < size)){
        curr = curr->next;
    }
    if(curr == &tail){
        return NULL;
    }
  
    curr->free = false;
    if(curr->size >= size + sizeof(struct block)){
        struct block *splitNode = (struct block * )((char* )curr + size + sizeof(struct block));
        splitNode->free=true;
        splitNode->size=curr->size - size - sizeof(struct block)*2;
        splitNode->next = curr->next;
        curr->next->prev = splitNode;
        splitNode->prev = curr;
        curr->next = splitNode;
    }
    curr->size = size;
    return curr;
}

