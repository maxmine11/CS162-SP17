/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#define METADATA_SIZE sizeof(memory_block)
memory_block *head_block = NULL;
memory_block *tail_block = NULL;


/* resquests memory size asked by  malloc and assigns the proper metadata to it
 * also returns a pointer to the metadata block 
 */
memory_block *memory_request(memory_block *previous, size_t size){
	memory_block *new_block;
	new_block = sbrk(0);
	void *block_size = sbrk(METADATA_SIZE + size);
	//assert((void*)new_block == block_size); // Need to make thread safe
	if(block_size == (void*)-1){
		return NULL; // We have reached the limit 
	}
	new_block->size = size; 
	new_block->free = 0;
	new_block->next = NULL;
	if (previous){
		previous->next=new_block;
	}
	new_block->prev= previous;
	// We are returning the begining of the metadata block. Return a pointer to the array[0] to  start at the memory block.
	return new_block;

}

/* Checks if there exists a free block inside of malloc  and if there is
 * stores the size in the given location where there's sufficient free space
 * and sets meta_block of memory as well.
 */
memory_block *checkfor_free_block(int which,size_t size){
	// which is used to prevent this function memsetting the data when using realloc
	memory_block *block;
	void *ptr;
	block = tail_block;
	size_t potential_size;
	memory_block *potential_block;
	void* potential_ptr=NULL;

	while(block != NULL){
		if (block->free && (block->size >= size)){
			if ((potential_size=block->size - size)>METADATA_SIZE){ // If there's space left biggger than the meta block split
				block->size = size; // Set this block's size to then new size
				potential_ptr = &block->start_point ;
				potential_ptr= potential_ptr + size;
				potential_block= potential_ptr;
				potential_block->size = potential_size-METADATA_SIZE;
				potential_block->free = 1;
				potential_block->next= block->next;
				potential_block->prev = block;
				block->next->prev = potential_block;
			}
			block->free = 0;
			ptr = &block->start_point;
			if (which == 1){
				memset(ptr, 0, block->size);
			}
			return block;
		}else{
			block = block->prev;
		}
	}
	return NULL; // No free space available
}
// Returns 0 if previous neighbor is free, 1 if next neighbor is free, 2 if both neighbors are free
// -1 if both neighbors are not free

int check_neighbors(memory_block *block){
	if(block->next && block->prev){
		if (block->next->free && block->prev->free){
			return 2;
		}
	}
	if(block->next){
		if (block->next->free)
		{
			return 1;
		}
	}
	if(block->prev){
		if (block->prev->free)
		{
			return 0;
		}
	}
	return -1;
	
}
	
void *mm_malloc(size_t size) {
	int malloc=1;
	memory_block *meta_block;
	void *block;
    /* YOUR CODE HERE */
    // No memory requested
    if (!size) {
    	return NULL;
    }
    if (!head_block){// First time malloc was called
    	meta_block = memory_request(NULL,size);
    	if (!meta_block){
    		return NULL ; // We failed to allocate memory in the heap
    	}else{
    		head_block = meta_block;
    		tail_block = meta_block;
    	}
    }else{
    	meta_block = checkfor_free_block(malloc,size);
    	if(meta_block== NULL){
    		meta_block= memory_request(tail_block,size);
    		if(meta_block==NULL){
    			return NULL;  // Failed to allocated memory
    		}
    		tail_block = meta_block; 
    	}
    }
    block = &meta_block->start_point;
    return block;

}

void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */
    void* dest;
    memory_block *check_block;
   	memory_block *block;
    block =(memory_block *) ptr-1;
    int realloc =2;
    if (ptr==NULL){ // Null pointer essentiallt calls malloc
    	mm_free(ptr);
    	if (size != 0){
    		dest = mm_malloc(size);
    		if (!dest){
    			return NULL;
    		} else{
    			return dest;
    		}
    	}else{
    		return NULL;
    	}
    }
    // Need to check if we have already enough space
    if(block->size >= size){
    	memset(ptr+size,0,block->size-size);
    	return ptr; // If there is return the same pointer
    }
    // First check if there's a free block we in the heap
    if(!(check_block=checkfor_free_block(realloc,size))){
    	// If we didn't find a free block then we call for more memory
    	check_block = memory_request(tail_block,size);
    	if(!check_block){
    		return NULL;
    	}
    	tail_block=check_block;
    }else{
    	dest = &check_block->start_point;
    	memset(dest,0,check_block->size);
    }

    dest=&check_block->start_point;
    memcpy(dest,ptr,check_block->size);
    return dest;

}

void mm_free(void *ptr) {
    /* YOUR CODE HERE */
    int neighbors;
    if(ptr==NULL){
    	return;
    }
    memory_block *block;
    block =(memory_block *) ptr-1;
   	assert(block->free == 0);
   	block->free = 1;
   	neighbors = check_neighbors(block);
   	if(neighbors==2){
   		//Both neighbors
   		block->size+=block->next->size + block->prev->size + 2*METADATA_SIZE; //Coalesce  the sizes of the 3 neighbors
   		if (block->next->next){
   			block->next->next->prev=block->prev; // Set this block's next neightbor's previous to this block's previous neighbor
   		}
   		if(block->prev->next){
   			block->prev->next=block->next->next; //Set this block's prev neightbor to this block's next neightbor's
   		}
   		block->prev->size = block->size;
   	}else if (neighbors == 1){
   		//Next neighbor
   		block->size+=block->next->size + METADATA_SIZE;
   		if(block->next->next){
   			block->next->next->prev=block; // This block's next neighbor's prev to point to this block
   			block = block->next->next;
   		}
   	}else if(neighbors==0){
   		//Previous neighbor
   		block->prev->size+=block->size + METADATA_SIZE;
   		block->prev->next=block->next; 
   		block->next = block->prev;
   	}
   	


}
