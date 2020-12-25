/*
 * =====================================================================================
 *
 *	Filename:  		sma.c
 *
 *  Description:	Base code for Assignment 3 for ECSE-427 / COMP-310
 *
 *  Version:  		1.0
 *  Created:  		6/11/2020 9:30:00 AM
 *  Revised:  		-
 *  Compiler:  		gcc
 *
 *  Author:  		Mohammad Mushfiqur Rahman
 *      
 *  Instructions:   Please address all the "TODO"s in the code below and modify 
 * 					them accordingly. Feel free to modify the "PRIVATE" functions.
 * 					Don't modify the "PUBLIC" functions (except the TODO part), unless
 * 					you find a bug! Refer to the Assignment Handout for further info.
 * =====================================================================================
 */

/* Includes */
#include "sma.h" // Please add any libraries you plan to use inside this file

/* Definitions*/
#define MAX_TOP_FREE (128 * 1024) // Max top free block size = 128 Kbytes
//	TODO: Change the Header size if required
#define FREE_BLOCK_HEADER_SIZE 24 // Size of the Header in a free memory block
//	TODO: Add constants here

typedef enum //	Policy type definition
{
	WORST,
	NEXT
} Policy;

char *sma_malloc_error;
void *freeListHead = NULL;			  //	The pointer to the HEAD of the doubly linked free memory list
void *freeListTail = NULL;			  //	The pointer to the TAIL of the doubly linked free memory list
unsigned long totalAllocatedSize = 0; //	Total Allocated memory in Bytes
unsigned long totalFreeSize = 0;	  //	Total Free memory in Bytes in the free memory list
Policy currentPolicy = NEXT;		  //	Current Policy
//	TODO: Add any global variables here
header_t *head = NULL; //header and tail of a singly linked list of all allocated blocks and free blocks
header_t *latestAllocation=NULL;

/*
 * =====================================================================================
 *	Public Functions for SMA
 * =====================================================================================
 */

/*
 *	Funcation Name: sma_malloc
 *	Input type:		int
 * 	Output type:	void*
 * 	Description:	Allocates a memory block of input size from the heap, and returns a 
 * 					pointer pointing to it. Returns NULL if failed and sets a global error.
 */
void *sma_malloc(int size)
{
	void *pMemory = NULL;

	// Checks if the free list is empty
	if (head == NULL)
	{
		// Allocate memory by increasing the Program Break
		pMemory = allocate_pBrk(size);
	}
	// If free list is not empty
	else
	{
		// Allocate memory from the free memory list
		pMemory = allocate_list(size);

		// If a valid memory could NOT be allocated from the free memory list
		if (pMemory == NULL)
		{
			// Allocate memory by increasing the Program Break
			pMemory = allocate_pBrk(size);
		}
	}
 
	// Validates memory allocation
	if (pMemory < 0 || pMemory == NULL)
	{
		sma_malloc_error = "Error: Memory allocation failed!";
		return NULL;
	}

	// Updates SMA Info
	totalAllocatedSize += size;

	return pMemory;
}

/*
 *	Funcation Name: sma_free
 *	Input type:		void*
 * 	Output type:	void
 * 	Description:	Deallocates the memory block pointed by the input pointer
 */
void sma_free(void *ptr)
{
	//	Checks if the ptr is NULL
	if (ptr == NULL)
	{
		puts("Error: Attempting to free NULL!");
	}
	//	Checks if the ptr is beyond Program Break
	else if (ptr > sbrk(0))
	{
		puts("Error: Attempting to free unallocated space!");
	}
	else
	{
		//	Adds the block to the free memory list
		free_block(ptr);
	}
}

/*
 *	Funcation Name: sma_mallopt
 *	Input type:		int
 * 	Output type:	void
 * 	Description:	Specifies the memory allocation policy
 */
void sma_mallopt(int policy)
{
	// Assigns the appropriate Policy
	if (policy == 1)
	{
		currentPolicy = WORST;
	}
	else if (policy == 2)
	{
		currentPolicy = NEXT;
	}
}

/*
 *	Funcation Name: sma_mallinfo
 *	Input type:		void
 * 	Output type:	void
 * 	Description:	Prints statistics about current memory allocation by SMA.
 */
void sma_mallinfo()
{
	//	Finds the largest Contiguous Free Space (should be the largest free block)
	int largestFreeBlock = get_largest_freeBlock();
	char str[60];

	//	Prints the SMA Stats
	sprintf(str, "Total number of bytes allocated: %lu", totalAllocatedSize);
	puts(str);
	sprintf(str, "Total free space: %lu", totalFreeSize);
	puts(str);
	sprintf(str, "Size of largest contigious free space (in bytes): %d", largestFreeBlock);
	puts(str);
}

/*
 *	Funcation Name: sma_realloc
 *	Input type:		void*, int
 * 	Output type:	void*
 * 	Description:	Reallocates memory pointed to by the input pointer by resizing the
 * 					memory block according to the input size.
 */
void *sma_realloc(void *ptr, int size)
{
	// TODO: 	Should be similar to sma_malloc, except you need to check if the pointer address
	//			had been previously allocated.
	// Hint:	Check if you need to expand or contract the memory. If new size is smaller, then
	//			chop off the current allocated memory and add to the free list. If new size is bigger
	//			then check if there is sufficient adjacent free space to expand, otherwise find a new block
	//			like sma_malloc.
	//			Should not accept a NULL pointer, and the size should be greater than 0.
	if(!ptr || size<=0) return NULL;
	header_t *header=(header_t*)ptr - 1;
	if (header->core.size >= size) return ptr; //todo: chop off excess memory
	void *result = malloc(size);
	if (result) {
		memcpy(result,ptr,header->core.size);
		free_block(ptr);
	}
	return result;
}

/*
 * =====================================================================================
 *	Private Functions for SMA
 * =====================================================================================
 */

void free_block(void *block) {
	header_t *header=(header_t*)block - 1;
	header->core.isFree=1;
	//looking for opportunities to merge adjacent free blocks
	header_t *prev=get_prev(header);
	header_t *next=header->core.next;
	header_t *beginning= ((prev && prev->core.isFree) ? prev:header);
	if(prev && prev->core.isFree) beginning=merge_free_blocks(beginning,header);
	if(next && next->core.isFree) beginning=merge_free_blocks(beginning,next);
	//if this merged block is big enough for sbrk(-n) reduction, do so
	if(!beginning->core.next && (beginning->core.size+sizeof(header_t)) > 1024*128) {
		if (!head->core.next) {
			head = NULL;
		} else {
			header_t *tmp = head;
			while (tmp) {
				if(tmp->core.next == beginning) {
					tmp->core.next = NULL;
					//tail = tmp;
				}
				tmp = tmp->core.next;
			}
		}
		sbrk(- (beginning->core.size+sizeof(header_t)) );
	}
}

//merges block2 into block1, helper method for free_block()
header_t *merge_free_blocks(header_t *header1, header_t *header2) {
	header_t *two_nh=header2->core.next;
	header1->core.next=two_nh;
	header1->core.size+=(header2->core.size+sizeof(header_t));
	header2->core.isFree=5; //for testing
	return header1;
}

header_t *get_prev(header_t *header) {
	header_t *tmp = head;
	while (tmp) {
		if(tmp->core.next == header) {
			return tmp;
		}
		tmp = tmp->core.next;
	}
	return NULL;
}

/*
 *	Funcation Name: allocate_pBrk
 *	Input type:		int
 * 	Output type:	void*
 * 	Description:	Allocates memory by increasing the Program Break
 */
void *allocate_pBrk(int size)
{
	int total_size = sizeof(header_t) + size;
	void *newBlock = sbrk(total_size); //could be split into two sbrk()'s in case something goes wrong
	if (newBlock == (void*) -1) return NULL;
	header_t *header=newBlock;
	header->core.size = size;
	header->core.isFree = 0;
	header->core.next = NULL;
	if (!head) head = header;
	get_tail()->core.next=header;
	//if (tail) tail->core.next = header;
	//tail = header;
	//adding the extra 127*1024 as a separate block
	header_t *header2=sbrk(127*1024);
	header2->core.size = 127*1024-sizeof(header_t);
	header2->core.isFree = 1;
	header2->core.next = NULL;
	header->core.next=header2;
	//tail->core.next = header2;
	//tail = header2;
	return (void*)(header + 1);
}

header_t *get_tail() {
	header_t *ptr = head;
	while(ptr) {
		if (!ptr->core.next) {
			return ptr;
		}
		ptr = ptr->core.next;
	}
	return NULL;
}

/*
 *	Funcation Name: allocate_freeList
 *	Input type:		int
 * 	Output type:	void*
 * 	Description:	Allocates memory from the free memory list
 */
void *allocate_list(int size)
{
	void *pMemory = NULL;

	if (currentPolicy == WORST)
	{
		// Allocates memory using Worst Fit Policy
		pMemory = allocate_worst_fit(size);
	}
	else if (currentPolicy == NEXT)
	{
		// Allocates memory using Next Fit Policy
		pMemory = allocate_next_fit(size);
	}
	else
	{
		pMemory = NULL;
	}

	return pMemory;
}

/*
 *	Funcation Name: allocate_worst_fit
 *	Input type:		int
 * 	Output type:	void*
 * 	Description:	Allocates memory using Worst Fit from the free memory list
 */
void *allocate_worst_fit(int size)
{
	header_t *ptr = head;
	header_t *largestBlock=NULL;
	while(ptr) {
		if(ptr->core.isFree==1 && ptr->core.size >= size && !largestBlock) {
			largestBlock=ptr;
		} else if(ptr->core.isFree==1 && ptr->core.size >= size && (ptr->core.size > largestBlock->core.size)) {
			largestBlock=ptr;
		}
		ptr = ptr->core.next;
	}
	if(largestBlock) {
		largestBlock->core.isFree=0;
		split_if_necessary(largestBlock,size);
		return (void*)(largestBlock + 1);
	} else {
		return NULL;
	}
}

/*
 *	Funcation Name: allocate_next_fit
 *	Input type:		int
 * 	Output type:	void*
 * 	Description:	Allocates memory using Next Fit from the free memory list
 */
void *allocate_next_fit(int size)
{
	header_t *ptr = (latestAllocation ? latestAllocation:head);
	while(ptr) {
		if (ptr->core.isFree && ptr->core.size >= size) {
			ptr->core.isFree=0;
			latestAllocation=ptr;
			split_if_necessary(ptr,size);
			return (void*)(ptr + 1);
		}
		ptr = ptr->core.next;
	}
	//another pass through the ones before latestAllocation
	ptr=head;
	while(ptr) {
		if (ptr->core.isFree && ptr->core.size >= size) {
			ptr->core.isFree=0;
			latestAllocation=ptr;
			split_if_necessary(ptr,size);
			return (void*)(ptr + 1);
		}
		ptr = ptr->core.next;
	}
	return NULL;
}

//helper for allocate_next_fit() and allocate_worst_fit(); splits a free block that is too big into two blocks
void split_if_necessary(header_t *header1,int size) {
	if(header1->core.size==size) return;
	header_t *header2=(header_t*)((char*)header1+sizeof(header_t)+size);
	header2->core.isFree=1;
	header2->core.next=header1->core.next;
	header2->core.size=header1->core.size-size-sizeof(header_t);
	header1->core.next=header2;
	header1->core.size=size;
}

/*
 *	Funcation Name: get_largest_freeBlock
 *	Input type:		void
 * 	Output type:	int
 * 	Description:	Extracts the largest Block Size
 */
int get_largest_freeBlock()
{
	int largestBlockSize = 0;

	//	TODO: Iterate through the Free Block List to find the largest free block and return its size
	header_t *ptr = head;
	while(ptr) {
		if (ptr->core.isFree && ptr->core.size > largestBlockSize) {
			largestBlockSize=ptr->core.size;
		}
		ptr = ptr->core.next;
	}

	return largestBlockSize;
}