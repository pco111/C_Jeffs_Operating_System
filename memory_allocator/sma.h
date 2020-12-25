/*
 * =====================================================================================
 *
 *  Filename:  		sma.h
 *
 *  Description:	Header file for SMA.
 *
 *  Version:  		1.0
 *  Created:  		3/11/2020 9:30:00 AM
 *  Revised:  		-
 *  Compiler:  		gcc
 *
 *  Author:  		Mohammad Mushfiqur Rahman
 *      
 *  Instructions:   Please address all the "TODO"s in the code below and modify them
 *                  accordingly. Refer to the Assignment Handout for further info.
 * =====================================================================================
 */

/* Includes */
//  TODO: Add any libraries you might use here.
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

//  Policies definition
#define WORST_FIT	1
#define NEXT_FIT	2

extern char *sma_malloc_error;

union header {
	struct {
		union header *next;
		size_t size;
		unsigned isFree;
	} core;
	char cushion[24]; 
}; 
typedef union header header_t;

//  Public Functions declaration
void *sma_malloc(int size);
void sma_free(void* ptr);
void sma_mallopt(int policy);
void sma_mallinfo();
void *sma_realloc(void *ptr, int size);

//  Private Functions declaration
static void* allocate_pBrk(int size);
static void* allocate_freeList(int size);
static void* allocate_worst_fit(int size);
static void* allocate_next_fit(int size);
static void allocate_block(void* newBlock, int size, int excessSize, int fromFreeList);
static void replace_block_freeList(void* oldBlock, void* newBlock);
static void add_block_freeList(void* block);
static void remove_block_freeList(void* block);
static int get_blockSize(void *ptr);
static int get_largest_freeBlock();
void *allocate_list(int size);
void free_block(void *block);
header_t *merge_free_blocks(header_t *header1, header_t *header2);
header_t *get_prev(header_t *header);
void split_if_necessary(header_t *header1,int size);
header_t *get_tail();
//  TODO: Declare any private functions that you intend to add in your code.