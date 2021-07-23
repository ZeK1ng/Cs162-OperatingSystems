/*
 * mm_alloc.h
 *
 * A clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>
#include "bool.h"

typedef struct block{
  struct block * prev;
  struct block * next; 
  bool free;
  size_t size;
};


void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);
void * add_to_list(size_t size);
void * find_ff(size_t size);
