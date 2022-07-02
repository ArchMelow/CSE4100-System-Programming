#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20181671",
    /* Your full name*/
    "Jaejin Lee",
    /* Your email address */
    "jlee4923@naver.com",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 // single word size in bytes
#define DSIZE 8 // double word size in bytes 
#define CHUNKSIZE (1<<12) // 2^12 bytes

#define MAX(x, y) ((x) > (y)? (x): (y))
#define MIN(x, y) ((x) > (y)? (y): (x))

// pack a size and allocated bit into a word      
#define PACK(size, alloc) ((size) | (alloc))

// read/write a word at address p
#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *) (p) = (val))

// read the size/allocated info from p
#define GET_SIZE(p) (GET(p) & ~0x7) // Size bits
#define GET_ALLOC(p) (GET(p) & 0x1) // LSB for checking allocation

// given block pointer, compute addresses of header/footer
#define HDRP(bp) ((char *) (bp) - WSIZE)
#define FTRP(bp) ((char *) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define FREE_PREV(bp) ((char *) (bp))
#define FREE_NEXT(bp) ((char *) (bp + WSIZE))

// given block pointer, compute addresses of previous/next block pointers
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))

// given block pointer, compute addresses of previous/next FREE block pointers
#define FREE_PREV_BLKP(bp) ((char *) GET(FREE_PREV(bp)))
#define FREE_NEXT_BLKP(bp) ((char *) GET(FREE_NEXT(bp)))

static char *heap_listp = 0; // first block pointer of the heap (prologue block)
static char **seg_listp = 0; // pointer for the seglist each entry containing the address value of free blocks of specific sizes.

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t req_size);
static void place(void *bp, size_t asize);
static void seg_insert_freeblock(void* bp);
static void seg_remove_freeblock(void* bp);

int mm_check()
{
	int seglist_maxsize = 16; //the maximum index of the seglist.
	int is_marked_as_free = 1;
	int is_all_valid = 1;
	//int overlaps = 0; 

	//Is every block in the freelist (seglist) marked as free?
	//Also, are they valid?
	void *size_group; //current size group in traversal.
	void *bp; //block pointer for the current block in traversal.
	printf("Seg Freelist Checking Process..\n");
	for (int i = 0; i < seglist_maxsize; i++) {
		size_group = seg_listp + i;
		if (GET(size_group)) { //if the list header is not empty
			bp = (void *) GET(size_group); //type-cast to void *
			for (; bp!=NULL; bp = FREE_NEXT(bp)) {
				//check if the free block is marked as free.
				if (GET_ALLOC(HDRP(bp)) != 0) {
					printf("block %p in size group %d is not marked as a free block\n", bp, i);
					is_marked_as_free = 0; //unset the flag
				}
				//check if bp is valid (same header/footer and allocation status)
				//is size bits in the header and footer the same?
				if (GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp))) {
					printf("block %p in size group %d has different header and footer size\n", bp, i);
					is_all_valid = 0; //unset the flag
				}
				//is allocation status of the header and footer set to 0?
				if (GET_ALLOC(HDRP(bp)) == 0 && GET_ALLOC(FTRP(bp)) == 0) {
					printf("block %p in size group %d has different header and footer allocation status\n", bp, i);
					is_all_valid = 0; //unset the flag
				}

				if (GET_SIZE(HDRP(bp)) % DSIZE != 0) {
					printf("block %p in size group %d is not doubly aligned (8 B)\n", bp, i);
				}
			}
		}
	}

	//Are there any contiguous free blocks that somehow escaped coalescing?
    	//Are the allocated blocks in the heap valid?
	
	printf("Heap Checking Process..\n");
	for (bp = heap_listp + DSIZE; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        	if (GET_ALLOC(HDRP(bp)) == 0) { //if the current block is a free block
			if (GET_ALLOC(HDRP(PREV_BLKP(bp))) == 0 || GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 0) {
				printf("There is a contiguous block near %p which escaped coalescing\n", bp);
			}
		}
		else { //check the validity of the allocated blocks in the heap
			if (GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp))) {
				printf("block %p has different header and footer size\n", bp);
			}
			if (GET_ALLOC(HDRP(bp)) == 0 && GET_ALLOC(FTRP(bp)) == 0) {
                                printf("block %p has different header and footer allocation status\n", bp);
                        }
                        if (GET_SIZE(HDRP(bp)) % DSIZE != 0) {
                                printf("block %p is not doubly aligned (8 B)\n", bp);
                        }
		}
    	} 
	
	if (is_marked_as_free && is_all_valid) {
                printf("Blocks are all marked as free and valid\n");
        }
	return 0;
}

int mm_init(void)
{
    int seglist_size = 16; //the maximum index of the seglist.
    //try extending the heap for availing the space for the seglist, the prologue header/ footer, and the epilogue block.
    if ((heap_listp = mem_sbrk(WSIZE*(seglist_size + 4))) == (void *) -1)
        return -1;

    for (int i = 0; i<seglist_size*WSIZE; i++) {
	*(heap_listp+i) = 0; //set all seglist words (storing address) to 0
    }
 
    seg_listp = (char **) (heap_listp);

    heap_listp += seglist_size * WSIZE; //move pointer of the heap
    PUT(heap_listp, 0); //padding for the alignment.
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // epilogue header
    heap_listp += (2*WSIZE); // make heap pointer point to the location between prologue footer and the epilogue header.

    // extend heap with a free block of size 4096 bytes (1024 words)
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize; // adjusted block size
    size_t extendsize; // the amount to extend the heap by if there's no fit
    char *bp;
	
    asize = ALIGN(size + SIZE_T_SIZE); //align block
    // ignore non-positive values
    if (size <= 0)
        return NULL;
	
    // adjust block size to include overhead and satisfy 8-byte alignment

    // search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // no fit found. extend the heap and place
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    //mm_check();
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(FREE_PREV(bp), 0);
    PUT(FREE_NEXT(bp), 0);
    seg_insert_freeblock(coalesce(bp));//After coalescing, insert freed block in the seglist
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize, alignedSize;
      	
    if (ptr == NULL) {
	return mm_malloc(size);
    } //if ptr == NULL, realloc works same as mm_malloc.
    
    if (size == 0) {
	mm_free(ptr);
	newptr = NULL;
	return newptr; // if size == 0, realloc works same as mm_free.
    }
    	
    //if the size is nonzero
    else {
	copySize = GET_SIZE(HDRP(oldptr));
	
	alignedSize = ALIGN(size + SIZE_T_SIZE);
 


	//printf("copysize : %d | alignedSize : %d | size : %d\n", copySize, alignedSize, size);
	if (alignedSize == copySize)
	  return ptr;
	
	else if (alignedSize < copySize) { //the block is shrunk while re-allocating the block.
          //first align the size we're allocating (copying) 
	  if (copySize - alignedSize >= 2*DSIZE) { //minimum block size = 16 B
		PUT(HDRP(oldptr), PACK(alignedSize, 1));
		PUT(FTRP(oldptr), PACK(alignedSize, 1)); //replace current block pointer to an alloc block of size, alignedSize.	
		//now next block is the remaining part of the old block (oldptr, ptr)
		PUT(HDRP(NEXT_BLKP(oldptr)), PACK(copySize - alignedSize, 0));
		PUT(FTRP(NEXT_BLKP(oldptr)), PACK(copySize - alignedSize, 0)); //assign remaining part of the oldptr to free block.
		seg_insert_freeblock(NEXT_BLKP(oldptr)); //insert newly made free block
	  	
		 //return the allocated block
	  }
	  return oldptr;
	  //copySize = size;
	}
	else { //check if the block can be expanded while re-allocating the block.
	
		//int prev_exists = (PREV_BLKP(oldptr) != (void*) 0);
		int next_exists = (NEXT_BLKP(oldptr) != (void*) 0);
		//print_block(PREV_BLKP(oldptr));
		//print_block(NEXT_BLKP(oldptr));
		//int coal_with_prev_block_size = 0;
	        int coal_with_next_block_size = 0;
	        //int coal_with_both_block_size = 0;
		/*if (prev_exists)
			coal_with_prev_block_size = GET_SIZE(PREV_BLKP(oldptr)) + copySize;
		*/
		if (next_exists)
			coal_with_next_block_size = GET_SIZE(HDRP(NEXT_BLKP(oldptr))) + copySize;
			
		/*if (prev_exists && next_exists)
			coal_with_both_block_size = coal_with_prev_block_size + coal_with_next_block_size - copySize;
		*/
		//check if previous block could be coalesced with the current block
		/*if (prev_exists && (coal_with_prev_block_size >= alignedSize) && (GET_ALLOC(PREV_BLKP(oldptr)) == 0)) {
			//if previous block size + oldptr block size is enough for the requested size and that block is unalloc
			PUT(HDRP(oldptr), PACK(copySize, 0)); //unallocate the block
			PUT(FTRP(oldptr), PACK(copySize, 0));
			seg_remove_freeblock(PREV_BLKP(oldptr)); //remove previous block (which is free) from the seglist.
			PUT(HDRP(PREV_BLKP(oldptr)), PACK(alignedSize, 1)); //allocate on previous block
			PUT(FTRP(PREV_BLKP(oldptr)), PACK(alignedSize, 1));
			PUT(HDRP(NEXT_BLKP(PREV_BLKP(oldptr))), PACK(coal_with_prev_block_size - alignedSize, 0)); //mark remaining block as unalloc
                        PUT(FTRP(NEXT_BLKP(PREV_BLKP(oldptr))), PACK(coal_with_prev_block_size - alignedSize, 0));

			if (coal_with_prev_block_size - alignedSize >= 2*DSIZE) { //minimum block = 16 B
				seg_insert_freeblock(NEXT_BLKP(PREV_BLKP(oldptr))); //insert the remaining unalloc block.in the seglist.
			}
			return PREV_BLKP(oldptr);
		}*/
		if (next_exists && (coal_with_next_block_size >= alignedSize) && (GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) == 0)) {
			//PUT(HDRP(oldptr), PACK(copySize, 0));
			//PUT(FTRP(oldptr), PACK(copySize, 0));
			seg_remove_freeblock(NEXT_BLKP(oldptr));
			PUT(HDRP(oldptr), PACK(coal_with_next_block_size, 1));
			PUT(FTRP(oldptr), PACK(coal_with_next_block_size, 1));
			//PUT(HDRP(NEXT_BLKP(ptr)), PACK(coal_with_next_block_size - alignedSize, 0));
			//PUT(FTRP(NEXT_BLKP(ptr)), PACK(coal_with_next_block_size - alignedSize, 0));

			//print_block(ptr);
                	//print_block(NEXT_BLKP(ptr));

			/*if (coal_with_next_block_size - alignedSize >= 2*DSIZE) {
				seg_insert_freeblock(NEXT_BLKP(ptr));
			}*/
			return oldptr;
		}

		/*else if (prev_exists && next_exists && (coal_with_both_block_size >= alignedSize) && (GET_ALLOC(PREV_BLKP(oldptr))) == 0 && (GET_ALLOC(NEXT_BLKP(oldptr))) == 0 ) {
			PUT(HDRP(oldptr), PACK(copySize, 0));
			PUT(FTRP(oldptr), PACK(copySize, 0));
			seg_remove_freeblock(PREV_BLKP(oldptr));
			seg_remove_freeblock(NEXT_BLKP(oldptr));
			PUT(HDRP(PREV_BLKP(oldptr)), PACK(alignedSize, 1));
			PUT(FTRP(PREV_BLKP(oldptr)), PACK(alignedSize, 1));
			PUT(HDRP(NEXT_BLKP(PREV_BLKP(oldptr))), PACK(coal_with_both_block_size - alignedSize, 0));
			PUT(FTRP(NEXT_BLKP(PREV_BLKP(oldptr))), PACK(coal_with_both_block_size - alignedSize, 0));

			if (coal_with_both_block_size - alignedSize >= 2*DSIZE) {
				seg_insert_freeblock(NEXT_BLKP(PREV_BLKP(oldptr)));
			}
			return PREV_BLKP(oldptr);
		}*/
		
	}
	newptr = mm_malloc(size);
        if (newptr == NULL)
          return NULL;
	memcpy(newptr, oldptr, copySize);
    	mm_free(oldptr);
    	return newptr;
    }
}



static void *extend_heap(size_t words) {
    size_t size; 
    char *bp;
    //allocate an even number of words to maintain double word alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //the size aligned to double-word alignment (8 B)
    
    if ((long) (bp = mem_sbrk(size)) == -1) //tries extension by the aligned size block
        return NULL; //sbrk failed.

    //Implemented in boundary tag scheme
    //initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0)); //new free block header
    PUT(FTRP(bp), PACK(size, 0)); //new free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //new epilogue header
 
    bp = coalesce(bp); //coalesce current block by considering 4 cases.
    seg_insert_freeblock(bp); //insert the block in seglist (find appropriate size)

    return bp;
}


/* Description of static void coalesce(void *bp)
 * coalesces seperated adjacent free blocks into a logically single block (possible cases : case 1, case 2, case 3, case 4)
 * returns block pointer void *bp
 */

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //allocated bit of the previous block
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //allocated bit of the next block
    size_t size = GET_SIZE(HDRP(bp)); //current block's size

    //case 1 : previous block and next block are both allocated.
    if (prev_alloc && next_alloc) {
        return bp;
    }

    //case 2 : previous block is allocated, but next block is free
    else if (prev_alloc && !next_alloc) {
        seg_remove_freeblock(NEXT_BLKP(bp)); //remove the free block from the free list to update the size of the free block.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //logically coalesce current block with the next block
        PUT(HDRP(bp), PACK(size, 0)); //set the size of the colaesced block (both in header and footer)
        PUT(FTRP(bp), PACK(size, 0));
    }

    //case 3 : previous block is free and the next block is allocated
    else if (!prev_alloc && next_alloc) {
        seg_remove_freeblock(PREV_BLKP(bp)); //remove the free block from the free list to update the size of the free block.
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); //logically coalesce current block with the next block
        PUT(FTRP(bp), PACK(size, 0)); //set the size of the colaesced block (both in header and footer)
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //previous block is coalesced with the current block.
        bp = PREV_BLKP(bp);
    }

    //case 4 : both are free, so merge them all
    else {
        seg_remove_freeblock(PREV_BLKP(bp)); //remove the free block from the free list to update the size of the free block.
        seg_remove_freeblock(NEXT_BLKP(bp)); //remove the free block from the free list to update the size of the free block.
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); //logically coalesce current block with the next block
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //set the size of the colaesced block (both in header and footer)
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); //set the size of the colaesced block (both in header and footer)
        bp = PREV_BLKP(bp);
    }

    return bp;
}




static void *find_fit(size_t req_size) {

    int max_listsize = 16;
    //int size_group = get_size_class(asize);    
    void *bp;
    int tmpSize = req_size; //get the size of the current block.
    int exceeds = 0; //if remainder occurs, then that block needs one level higher than that of the current size group.
    int size_group = 0; //counting where the block belongs in seglist.
     
    for (; tmpSize > 16 && size_group < 15; tmpSize /= 2, size_group++) {
        //printf("tmpsize : %d\n", tmpSize);
        if (tmpSize % 2)
        	exceeds = 1; //set the exceeds bit if a remainder occurs when divided by 2
    }

    if (exceeds && tmpSize == 16 && size_group < 15)
        size_group++; //one level higher than the computed group

    char **ptr;
    for (int i = size_group; i < max_listsize; i++) {
	    ptr = seg_listp + i;
	    if (*ptr) {
		 bp = (void *)GET(ptr);
		 for (;bp!=NULL; bp = FREE_NEXT_BLKP(bp)) {
			if (GET_SIZE(HDRP(bp)) >= req_size) {
			       return bp; //find first fit
			}
		 }
	   }
    }	   
 
    return NULL; // no fit found
}

static void place(void *bp, size_t asize) {

    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= 2*DSIZE) { 

        seg_remove_freeblock(bp); //the remaining free block can be inserted into the seglist, so remove the whole free block first.

	// the part for placing the block into a free block when asize <= csize
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        // splice out prev and next free block from the seglist to insert the block
        PUT(FREE_PREV(bp), 0);
        PUT(FREE_NEXT(bp), 0);

        seg_insert_freeblock(bp); //insert a new block into the seglist.
    } else {
        seg_remove_freeblock(bp); //remove the whole free block from the seglist first.
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void seg_insert_freeblock(void *bp) {
    
    size_t size = GET_SIZE(HDRP(bp)); // adjusted size
    char **sizegroup_header; // the pointer to the address of the first free block of the size class
    int exceeds = 0;
    int size_group = 0;
    int tmpSize = size;

    for (; tmpSize > 16 && size_group < 15; tmpSize /= 2, size_group++) {
        //printf("tmpsize : %d\n", tmpSize);
        if (tmpSize % 2)
                exceeds = 1; //set the exceeds bit if a remainder occurs when divided by 2
    }

    if (exceeds && tmpSize == 16 && size_group < 15)
        size_group++; //one level higher than the computed group

    sizegroup_header = seg_listp + size_group;
 
    if (!(*sizegroup_header)) { // if the header for a specific size group of the seglist is empty..
        PUT(sizegroup_header, (unsigned int) bp); //let the header point to the new free block being inserted (bp)
        PUT(FREE_PREV(bp), (unsigned int) sizegroup_header); //let bp->prev point to the header 
        PUT(FREE_NEXT(bp), 0); //let bp->next point NULL (end of the list)
    }
    
    else { // if the header for a specific size group of the seglist is not empty..
        PUT(FREE_PREV(bp), (unsigned int) sizegroup_header); //let bp->prev point to the header
        PUT(FREE_NEXT(bp), GET(sizegroup_header)); //let bp->next point to the next block of the header
        PUT(FREE_PREV(GET(sizegroup_header)), (unsigned int) bp); //let (the next block of the header)->prev point to bp
        PUT(sizegroup_header, (unsigned int) bp); //let header point to bp
    }
}

static void seg_remove_freeblock(void *bp) {
 
    //need to check whether the previous block and the next block exists in free list.
    //If the previous block exists, it will point to the heap space outside the seglist. If not, it points to the ptr in seglist.
    int seglist_size = 16;
    void *prev = FREE_PREV_BLKP(bp); //points to the previous block in the seglist header	
    void *next = FREE_NEXT_BLKP(bp); //points to the next block in the seglist header
    int prev_exists = 0, next_exists = 0;


    unsigned int list_start_addr = (unsigned int) seg_listp;
    unsigned int list_end_addr = (list_start_addr + (unsigned int)((seglist_size - 1)*WSIZE));
 
    //determine if the previous block exists and set flag accordingly.
    if ((unsigned int) prev < list_start_addr || (unsigned int) prev > list_end_addr) {
	prev_exists = 1;
    }
    else if ((unsigned int) (list_end_addr - (unsigned int) prev) % WSIZE) {
	prev_exists = 1;
    }
    if (next) 
	next_exists = 1;
    
    if (GET_ALLOC(HDRP(bp))) {
        return;
    } // if bp is allocated, do not remove block. (wrong operation)

    if (!prev_exists && !next_exists) { // case 1 : if this is the first block in the seglist header
        PUT(prev, (unsigned int) next);
    }

    else if (!prev_exists && next_exists) { // case 2 : if this is the first block, and has a block next to it
        PUT(prev, (unsigned int) next);
        PUT(FREE_PREV(next), (unsigned int) prev);
    }

    else if (prev_exists && next_exists) { // case 3 : if this is not the first block, and has a block next to it
        PUT(FREE_NEXT(prev), (unsigned int) next);
        PUT(FREE_PREV(next), (unsigned int) prev);
    }

    else { //case 4 : if this is the last block in the seglist header
        PUT(FREE_NEXT(prev), 0);
    }

    //finally, splice out previous connections with bp
    PUT(FREE_PREV(bp), 0);
    PUT(FREE_NEXT(bp), 0);
}


