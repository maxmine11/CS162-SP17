/*
 * mm_alloc.h
 *
 * A clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>


typedef struct memory_block{
	size_t size;
	int free;
	struct memory_block *next;
	struct memory_block *prev;	
	int start_point[0];
} memory_block;

void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);
