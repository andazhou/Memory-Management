/*
 * Dynamic memory allocator with explicit free lists,
 * segregated starting at 2^5 size, boundary tag coalescing,
 * FIFO placement, and doubleword aligned.
 */


/* Author: Anda Zhou
 * AndrewID: azhou
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mm.h"
#include "memlib.h"

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* For debugging */
#ifdef DEBUG
#define CHECKHEAP(verbose) \
    printf("%s line %d\n", __func__, __LINE__); \
    mm_checkheap(lineno);
#else
#define CHECKHEAP(verbose)
#endif

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  170  /* Extend heap by this amount (bytes) */
#define BUCKETS    12  /* Number of buckets for segregated list */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block pointer bp, finds pointer to next & previous free blocks
 * in free list
 */
#define NEXT_FREE(bp)  (*((char **)((char *)(bp) + DSIZE)))
#define PREV_FREE(bp)  (*((char **)(char *)(bp)))



/* Allocated block structure:
 * |HEAD|PAYLOAD|FOOT|
 * |HEAD| = header with size and allocation bit
 *      |PAYLOAD| = data; prev/next when free
 *              |FOOT| = footer with size & allocation bit
 */


/* Free list structure:
 * [BUCKET] |HEAD|PREV|NEXT|FREE|FOOT|
 *          |HEAD| = header
 *               |PREV| = previous free block
 *                    |NEXT| = next free block
 *                         |FREE| = unused
 *                              |FOOT| = footer
 */


/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */

/* Segregated Free List */
static char **seg_free;

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void insert_free(void *bp);
void check_block(void* bp);
static void remove_free(void *bp);
static void printblock(void *bp);
int find_bucket(size_t size);
void cycle_check(void* bp);


/*
 * mm_init - Initialize the memory manager: creates initial heap
 * and pointers at beginning for each bucket in segregated free
 * list
 */

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE + (BUCKETS * DSIZE))) == (void *)-1)
        return -1;

    /* Set seg_free to beginning of heap */
    seg_free = (char **)heap_listp;
    for (int i = 0; i < BUCKETS; i++) {
        seg_free[i] = NULL;
    }

    /* Move heap pointer past seg_free space */
    heap_listp += BUCKETS * DSIZE;

    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}
/* end mm_init */


/*
 * malloc - Allocate a block with at least size bytes of payload.
 * Searches free list for fit, if no fit found, extends heap and
 * allocates more memory.
 */

void *malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    if (heap_listp == 0){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 3*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);

    return bp;
}

/* end malloc */


/*
 * free - Free a block. Find appropriate free list
 * and place block if free list is empty, else
 * coalesce(bp) places block after coalescing.
 */

void free(void *bp)
{
    if (bp == 0)
        return;

    size_t size = GET_SIZE(HDRP(bp));

    if (heap_listp == 0){
        mm_init();
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    int bindex = find_bucket(size);
    /* If block is first in its free list, place */
    if (seg_free[bindex] == NULL) insert_free(bp);

    /* else coalesce(bp) will check for adjacent free & place */
    else coalesce(bp);
}
/* end free */


/*
 * realloc - Returns pointer to allocated space of size bytes
 * if *ptr is NULL, this is malloc
 * if size == 0, this is free
 * else takes ptr's memory and allocates memory to hold it
 * and returns ptr to new block
 */

void *realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

/* end realloc */


/*
 * mm_checkheap - Check the heap for correctness. Checks
 * overall heap as well as segregated free list and all
 * memory blocks. Only prints with DEBUG & if an error is
 * found.
 * Helper functions include: check_block, cycle_check
 */

void mm_checkheap(int lineno)
{
    /* Checking overall heap */
    char* bp = heap_listp;
    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
        printf("Heap error: prologue header\n");
    }

    /* Checking each block */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        check_block(bp);
    }

    /* Checking end of heap */
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
        printf("Heap error: epilogue header\n");
    }

    /* Seg_list check */
    for (int i = 0; i < BUCKETS; i++) {
        char* flp = seg_free[i];

        /* Check for cycles in each bucket list */
        cycle_check(flp);

        for (flp = seg_free[i]; flp != NULL; flp = NEXT_FREE(flp)) {

            /* is prev(next(bp)) bp? */
            if (NEXT_FREE(flp) != NULL && PREV_FREE(NEXT_FREE(flp)) != flp) {
                printf("Links in linked list do not match\n");
            }

            /* is next(prev(bp)) bp itself? */
            if (PREV_FREE(flp) != NULL && NEXT_FREE(PREV_FREE(flp)) != flp) {
                printf("Links in linked list do not match\n");
            }

            /* Are blocks in the right size bucket? */
            if (find_bucket(GET_SIZE(flp)) != i) {
                printf("Blocks are not in correct seg_list\n");
            }

            /* Free blocks only. Is block allocated? */
            if (GET_ALLOC(flp)) {
                printf("Allocated block in free list\n");
            }
        }
    }
}

/* end mm_checkheap */


/* cycle_check - Tortoise & hare algorithm for detecting cycles in
 * linked lists. Tortoise moves one link forward each time,
 * hare moves two. If tortoise = hare, there is a cycle. If
 * tortoise reaches end of list, there is no cycle.
 */

void cycle_check(void* bp)
{
    void* hare = bp;
    void* tortoise = bp;

    /* tortoise moves one step at a time */
    for (tortoise = bp; tortoise != NULL; tortoise = NEXT_FREE(tortoise)) {

        /* hare skips 2 links at a time */
        hare = NEXT_FREE(NEXT_FREE(hare));

        /* if they meet, there is a cycle */
        if (tortoise == hare) {
            printf("Cycle detected in linked list\n");
        }
    }
}

/* end cycle_check */



/* check_block - Checks each block in heap for alignment & correctness */
void check_block(void* bp)
{
    size_t header = GET_SIZE(HDRP(bp));
    size_t footer = GET_SIZE(FTRP(bp));

    /* Do header & footer match? */
    if ((header != footer) || GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp))) {
        printf("Header & footer do not match\n");
    }

    /* Alignment/size check */
    if ((header % DSIZE != 0) || header < DSIZE) {
        printf("Block size error\n");
    }

    /* Are there consecutive free blocks? Coalesce check */
    if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
        printf("Consecutive free blocks\n");
    }

    /* Is this block pointer in the heap? Segfault check */
    if ((bp > mem_heap_hi() || bp < mem_heap_lo())) {
            printf("Bp out of heap bounds\n");
    }

}

/* end check_block */



/*
 * extend_heap - Extend heap with free block and return its block pointer
 */

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free, adds to free list */
    return coalesce(bp);
}

/* end extend_heap */



/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block.
 * Case 1: |ALLOC|bp|ALLOC|
 * Case 2: |ALLOC|bp|FREE| - coalesce next free
 * Case 3: |FREE|bp|ALLOC| - coalesce prev
 * Case 4: |FREE|bp|FREE| - coalesce prev & next
 */

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));


    if (prev_alloc && next_alloc) {            /* Case 1 */
        /* Nothing needs coalescing */
        size = size;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        /* Include size of next */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        /* Remove next free - keep bp */
        remove_free(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        /* Include size of prev */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        /* Remove prev free and move bp back */
        remove_free(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                      /* Case 4 */
        /* Include size of both prev & next */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_free(PREV_BLKP(bp));
        remove_free(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        /* Set bp to previous */
        bp = PREV_BLKP(bp);
    }

    /* Insert coalesced free block */
    insert_free(bp);
    return bp;
}

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_free(bp);

    /* Is rest of block enough for another block? */
    if ((csize - asize) >= (3*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        insert_free(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* end place */


/*
 * find_fit - Find a fit for a block with asize bytes
 * Determine which bucket to place in based on size,
 * search free list for a block >= size
 */

static void *find_fit(size_t asize)
{
    void *bp;
    int bsize = find_bucket((int)asize);

    /* Start searching seg_free at bucket and up */
    for (int i = bsize; i < BUCKETS; i++) {
        for (bp = seg_free[i]; bp != NULL; bp = NEXT_FREE(bp)) {
            if (asize <= GET_SIZE(HDRP(bp))) {
                return bp;
            }
        }
    }
    return NULL;

}

/* end find_fit */


/* insert_free - Inserts free block into appropriate
 * seg_list bucket and modifies pointers in that
 * list to accommodate new block.
 */

static void insert_free(void *bp)
{
    int bsize = find_bucket(GET_SIZE(HDRP(bp)));

    /* First block in its list */
    if (seg_free[bsize] == NULL) {
        PREV_FREE(bp) = NULL;
        NEXT_FREE(bp) = NULL;
    }

    /* Link new block to front of list */
    else {
        PREV_FREE(seg_free[bsize]) = bp;
        NEXT_FREE(bp) = seg_free[bsize];
        PREV_FREE(bp) = NULL;
    }

    /* Point list to bp */
    seg_free[bsize] = bp;
}

/* end insert_free */


/* remove_free - Removes free block from its seg_list
 * and modifies pointers.
 */

static void remove_free(void *bp)
{
    int bucket = find_bucket(GET_SIZE(HDRP(bp)));

    /* Case 1: only block in list */
    if (PREV_FREE(bp) == NULL && NEXT_FREE(bp) == NULL) {
        seg_free[bucket] = NULL;
    }

    /* Case 2: first block in list */
    else if (PREV_FREE(bp) == NULL) {
        PREV_FREE(NEXT_FREE(bp)) = NULL;
        seg_free[bucket] = NEXT_FREE(bp);
    }

    /* Case 3: last block in list */
    else if (NEXT_FREE(bp) == NULL) {
        NEXT_FREE(PREV_FREE(bp)) = NULL;
    }

    /* Case 4: block somewhere in middle */
    else {

        void* prev = PREV_FREE(bp);
        void* next = NEXT_FREE(bp);

        NEXT_FREE(prev) = next;
        PREV_FREE(next) = prev;

    }
}

/* end remove_free */


/* printblock - prints size of header & footer for debugging */

static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'),
           fsize, (falloc ? 'a' : 'f'));
}

/* end printblock */


/* find_bucket - Determines which bucket free blocks should go into
 * Bucket 0 includes sizes up to 32, bucket sizes increment
 * with power of 2 up to defined number of BUCKETS.
 */

int find_bucket(size_t size)
{
    int bucket = 0;
    int bsize = (int)size;
    for (int i = 0; i < BUCKETS; i++) {
        if (bsize >= (1 << (i + 4))) bucket = i;
    }
    bucket = bucket;
    return bucket;
}

/* end find_bucket */


/* END OF FILE */

