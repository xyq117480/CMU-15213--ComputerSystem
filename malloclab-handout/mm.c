/*
 * mm.c
 *
 * Name: Aihua Peng
 * AndrewID: aihuap
 *
 * This solution is a combination of segregated list and first fit.
 * The entire heap consists of a padding block, a pair of prologue
 * header and footer, link list headers, payload and a epilogue block.
 * Free blocks are connected by different free lists, removed from or inserted
 * into the list due to its size.
 * Each block has 3 bit fields to indicate the allocation of next, previous,
 * and itself.
 * Each free list is starting at the initial heap, and also ending there.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */


/* single word (4) or double word (8) alignment */
#define ALIGN(p) (((size_t)(p) + (DSIZE - 1)) & ~0x7)
#define ALIGNED(p)  ((size_t)ALIGN(p) == (size_t)p)

/* Basic sizes */
#define ALIGNMENT   8          /* Dword alignment */
#define WSIZE       4          /* Word and header/footer size (bytes) */
#define DSIZE       8          /* Double word size (bytes) */
#define CHUNKSIZE   (1 << 9)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, next_alloc, prev_alloc, alloc)  ((size) | (next_alloc)| (prev_alloc) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address ,size and allocated fields of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given hdr ptr p, get and set alloc field from addres p */
#define GET_PREV_ALLOC(p) (GET(p) & 0x2)
#define SET_PREV_ALLOC(p) (PUT(p, (GET(p) | 0x2 )));
#define GET_NEXT_ALLOC(p) (GET(p) & 0x4)
#define SET_NEXT_ALLOC(p) (PUT(p, (GET(p) | 0x4 )));
#define SET_PREV_FREE(p)  (PUT(p, (GET(p) & (~0x2))));
#define SET_NEXT_FREE(p)  (PUT(p, (GET(p) & (~0x4))));

/* Given block ptr bp, get the pointer of the address of next/prev free block */
#define NEXT_P(bp) (bp)
#define PREV_P(bp) ((char *)bp + WSIZE)

/* Given block ptr bp, get the pointer of next/prev free block */
#define NEXT_FREE_P(bp) (heap_basep + *(unsigned int *)NEXT_P(bp))
#define PREV_FREE_P(bp) (heap_basep + *(unsigned int *)PREV_P(bp))

/* Size classes */
#define SIZE1 (1<<4)
#define SIZE2 (1<<5)
#define SIZE3 (1<<6)
#define SIZE4 (1<<7)
#define SIZE5 (1<<8)
#define SIZE6 (1<<9)
#define SIZE7 (1<<10)
#define SIZE8 (1<<11)
#define SIZE9 (1<<12)
#define SIZE10 (1<<13)
#define SIZE11 (1<<14)

#define LIST_NUM 12         /* The number of lists */


/* Global variants */
static char *heap_listp = 0;
static char *heap_basep = 0;

/* Helpers */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void insert_block(void *bp, size_t index);
static void delete_block(void *bp);
static size_t get_index(size_t size);

void checkHeapStructure();
void checkEachFreeBlockInList(void *listPtr);
void checkEachBlockInPayload(void *payloadPtr);
/* Check if in heap */
static inline int in_heap(const void* p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}


/*
 * mm_init - Initialize function, allocate prologue, epilogue and
 * the lists' start points. Return -1 on error, 0 on success.
 */

int mm_init(void){
    size_t sizeForInit = (LIST_NUM * 2 + 4) * WSIZE;
    if ((heap_listp = mem_sbrk(sizeForInit)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);     /* Blank block for aligning */
    PUT(heap_listp + (1 * WSIZE), PACK(sizeForInit - DSIZE,0,2,1));  /* Prologue header */
    PUT(heap_listp + sizeForInit - DSIZE, PACK(sizeForInit - DSIZE,0,2,1)); /* Prologue footer */
    PUT(heap_listp + sizeForInit - WSIZE, PACK(0, 0,2, 1)); /* Epilogue header */
    heap_basep = heap_listp;         /* Set base pointer */
    heap_listp = heap_listp + DSIZE; /* Move listp to the beginning of list */
    
    for (size_t i = 0; i < LIST_NUM; i++) {
        PUT(heap_listp + i * DSIZE, PACK(i*DSIZE+DSIZE, 0,0,0));
        PUT(heap_listp + i * DSIZE + WSIZE, PACK(i*DSIZE+DSIZE, 0,0,0));
    }
    if (extend_heap(CHUNKSIZE * 8 / WSIZE) == NULL){
        return -1;
    }
    return 0;
}


/*
 * Malloc - Based on user's request, malloc function asks the required space in the existing 
 * heap or ask for more space if necessary. Returns a pointer to the allocated block on success,
 * null on special cases.
 */

void *malloc(size_t size){
    size_t asize;
    size_t extendsize;
    char *bp;
    
    if (size <= 0) {
        return NULL;
    }
    
    if (size <= DSIZE + WSIZE) {
        asize = DSIZE * 2;
    }
    else{
        
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    }
    
    if ((bp=find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    
    extendsize = MAX(asize, CHUNKSIZE);
    
    if ((bp = extend_heap(extendsize/WSIZE))==NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
    
}

/*
 * free - Free the block located by the pointer.
 */
void free(void *ptr){
    if(!ptr)
        return;
    
    size_t size = GET_SIZE(HDRP(ptr));
    size_t _prevAlloc = GET_PREV_ALLOC(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0,_prevAlloc, 0));
    PUT(FTRP(ptr), PACK(size, 0,_prevAlloc, 0));
    coalesce(ptr);
}

/*
 * realloc - Based on the location (ptr) and size (size) request, it returns at least that 
 * size amount of space. The extra space may be caused by padding. When no loation is 
 * specified, it just works as malloc.
 */
void *realloc(void *ptr, size_t size){
    
    size_t _newSize = 0;
    size_t _oldSize = 0;
    size_t _asize = 0;
    size_t _prevAlloc = 0;
    void *_ptr;
    if (!ptr) {
        return malloc(size);
    }
    if (!size) {
        free(ptr);
        return NULL;
    }
    _oldSize = GET_SIZE(HDRP(ptr));
    _prevAlloc = GET_PREV_ALLOC(HDRP(ptr));
    if (size <= (DSIZE + WSIZE)){
        _newSize = 2 * DSIZE;
    }
    else{
        _newSize = ALIGN(size + WSIZE);
    }
    
    if (_newSize <= _oldSize){
        return ptr;
    }
    else {
        if (_asize >= _newSize) {
            if (_asize - _newSize >= 2 * DSIZE){
                if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr)))) {
                    _asize = _oldSize + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
                    delete_block(NEXT_BLKP(ptr));
                    PUT(HDRP(ptr), PACK(_newSize, 0,_prevAlloc, 1));
                    PUT(HDRP(NEXT_BLKP(ptr)), PACK(_asize- _newSize,0, 2, 0));
                    PUT(FTRP(NEXT_BLKP(ptr)), PACK(_asize -_newSize,0, 2, 0));
                    SET_PREV_ALLOC(HDRP(NEXT_BLKP(NEXT_BLKP(ptr))));
                    insert_block(NEXT_BLKP(ptr), get_index(_asize - _newSize));
                }
                else{
                    PUT(HDRP(ptr), PACK(_asize,0, _prevAlloc, 1));
                    SET_PREV_ALLOC(HDRP(NEXT_BLKP(ptr)));
                }
                return ptr;
            }
        }
    }
    _ptr = malloc(size);
    if (!_ptr){
        return NULL;
    }
    memcpy(_ptr, ptr, _oldSize);
    free(ptr);
    return _ptr;
}

/*
 * calloc - Ask for space for nmemb number of size bytes. Returns a pointer to 
 * the allocated memorty.
 */
void *calloc(size_t nmemb, size_t size){
    size_t bytes = nmemb * size;
    void *newptr;
    
    newptr = malloc(bytes);
    memset(newptr, 0, bytes);
    
    return newptr;
}


/*
 * extend_heap - extend the heap and the unit is word. Return a ptr to the extended memory
 * on success, NULL on error.
 */
static void *extend_heap(size_t words){
    char *bp;
    size_t size;
    size_t prev_alloc = 0;
    
    /* align size to multiple of 8 bytes */
    size = (words % 2) ? (words + 1) * WSIZE : (words * WSIZE);
    
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }
    
    /* new free block */
    prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    
    PUT(HDRP(bp), PACK(size,0, prev_alloc, 0));
    PUT(FTRP(bp), PACK(size,0, 0, 0));
    
    /* epilogue header */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,0, prev_alloc, 1));
    
    return coalesce(bp);
}

/*
 * coalesce - Deals with the 4 situations of block and its neighbors.
 * When a block and its prev/succ are both free, coalesce them.
 *
 */

static void *coalesce(void *bp){
    size_t _prevAlloc = GET_PREV_ALLOC(HDRP(bp));
    size_t _nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t _asize = GET_SIZE(HDRP(bp));
    
    if (_prevAlloc && _nextAlloc) {
        SET_PREV_FREE(HDRP(NEXT_BLKP(bp)));
    }
    else if (_prevAlloc && !_nextAlloc) {
        _asize += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(_asize,0, _prevAlloc, 0));
        PUT(FTRP(bp), PACK(_asize,0, _prevAlloc, 0));
    }
    else if (!_prevAlloc && _nextAlloc) {
        _asize += GET_SIZE(HDRP(PREV_BLKP(bp)));
        delete_block(PREV_BLKP(bp));
        SET_PREV_FREE(HDRP(NEXT_BLKP(bp)));
        PUT(FTRP(bp), PACK(_asize, 0,GET_PREV_ALLOC(HDRP(PREV_BLKP(bp))), 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(_asize, 0,GET_PREV_ALLOC(HDRP(PREV_BLKP(bp))), 0));
        
        bp = PREV_BLKP(bp);
    }
    else {
        _asize += GET_SIZE(HDRP(PREV_BLKP(bp))) +
        GET_SIZE(FTRP(NEXT_BLKP(bp)));
        delete_block(NEXT_BLKP(bp));
        delete_block(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(_asize,0, GET_PREV_ALLOC(HDRP(PREV_BLKP(bp))), 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(_asize,0, GET_PREV_ALLOC(HDRP(PREV_BLKP(bp))), 0));
        bp = PREV_BLKP(bp);
    }
    
    insert_block(bp, get_index(_asize));
    return bp;
}




/*
 * find_fit - Search a free list based on the size provided, which might provide enough
 * spaces. If this free list has no such a block, go to next list.
 */

static void *find_fit(size_t size){
    char * _currentList = heap_basep + get_index(size) * DSIZE;
    void * _nextFree;
    
    while (_currentList!=FTRP(heap_listp)) {
        _nextFree = NEXT_FREE_P(_currentList);
        while (_nextFree!=_currentList) {
            if (size <= GET_SIZE(HDRP(_nextFree))) {
                return _nextFree;
            }
            _nextFree = NEXT_FREE_P(_nextFree);
        }
        _currentList += DSIZE;
    }
    return NULL;
    
}

/* 
 * place - Put a block to location bp. Delete such bp from free list. The the rest
 * of space is big enought, split it.
 */
static void place(void *bp, size_t size){
    size_t _freeSize = GET_SIZE(HDRP(bp));
    delete_block(bp);
    if (_freeSize - size >= 2 * DSIZE) {
        PUT(HDRP(bp), PACK(size,0, GET_PREV_ALLOC(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(size,0, GET_PREV_ALLOC(HDRP(bp)), 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(_freeSize-size,0, 2, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(_freeSize-size,0, 2, 1));
        insert_block(NEXT_BLKP(bp), get_index(_freeSize - size));
    }
    else{
        PUT(HDRP(bp), PACK(_freeSize, 0,GET_PREV_ALLOC(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(_freeSize, 0,GET_PREV_ALLOC(HDRP(bp)), 1));
        SET_PREV_ALLOC(HDRP(NEXT_BLKP(bp)));
    }
}


/* 
 * insert_block - Insert node to a certain free list based on the index.
 */
static inline void insert_block(void *bp, size_t index){
    PUT(NEXT_P(bp), GET(NEXT_P(heap_listp + index * DSIZE)));
    PUT(PREV_P(bp), GET(PREV_P(NEXT_FREE_P(bp))));
    PUT(NEXT_P(heap_listp + index * DSIZE), (long)bp - (long)heap_basep);
    PUT(PREV_P(NEXT_FREE_P(bp)), (long)bp - (long)heap_basep);
}

/* 
 * delete_block - Delete the node from a free list.
 */
static inline void delete_block(void *bp){
    
    PUT(PREV_P(NEXT_FREE_P(bp)), GET(PREV_P(bp)));
    PUT(NEXT_P(PREV_FREE_P(bp)), GET(NEXT_P(bp)));
}


/* 
 * get_index - Get the index of free list based on size.
 */
static size_t get_index(size_t size){
    if (size <= SIZE1) {
        return 0;
    }
    if (size <= SIZE2) {
        return 1;
    }
    if (size <= SIZE3) {
        return 2;
    }
    if (size <= SIZE4) {
        return 3;
    }
    if (size <= SIZE5) {
        return 4;
    }
    if (size <= SIZE6) {
        return 5;
    }
    if (size <= SIZE7) {
        return 6;
    }
    if (size <= SIZE8) {
        return 7;
    }
    if (size <= SIZE9) {
        return 8;
    }
    if (size <= SIZE10) {
        return 9;
    }
    if (size <= SIZE11) {
        return 10;
    }
    else return 11;
}

/* 
 * mm_checkheap - Heap consistency checker, responsible for both heap check and
 * list check.
 */
int mm_checkheap(int verbose) {
    if (verbose) {
        printf("Heap address %p: \n", heap_listp);
    }
    checkHeapStructure();
    char *bp = heap_listp;
    for (int i = 0; i<LIST_NUM; i++) {
        checkEachFreeBlockInList(bp+i*DSIZE);
    }
    char *heap_payloadp = NEXT_BLKP(heap_listp);
    while (GET_SIZE(HDRP(heap_payloadp))!=0) {
        checkEachBlockInPayload(heap_payloadp);
        heap_payloadp = NEXT_BLKP(heap_payloadp);
    }
    return 0;
}

/*
 * checkHeapStructure - Check the structure correctness of the entire heap.
 */
void checkHeapStructure(){
    if (GET(heap_basep)) {
        printf("The alignment word is not correct (not zero). \n");
    }
    if (!GET_ALLOC(HDRP(heap_listp))||!GET_PREV_ALLOC(HDRP(heap_listp))) {
        printf("The prologue header bit fields are not correct. \n");
    }
    if (!GET_ALLOC(FTRP(heap_listp))||!GET_PREV_ALLOC(FTRP(heap_listp))) {
        printf("The prologue footer bit fields are not correct. \n");
    }
    if (GET_SIZE(HDRP(heap_listp))!=(LIST_NUM-1)*DSIZE) {
        printf("The number of lists is not correct. \n");
    }
    
}

/*
 * checkEachFreeBlockInList - check block in list one by one, dealing with
 * align, next/prev consistency, in-range, header/footer consistency, etc..
 */
void checkEachFreeBlockInList(void *listPtr){
    char *_currentList = listPtr;
    void *_nextBlock = NEXT_FREE_P(_currentList);
    size_t max = GET_SIZE(HDRP(listPtr));
    size_t min = max >> 1;
    while (_nextBlock!=_currentList) {
        if (!ALIGNED(_nextBlock)) {
            printf("Align issue within block[%p] in list[%p]. \n", _nextBlock, listPtr);
        }
        if (PREV_FREE_P(NEXT_FREE_P(_nextBlock)) != _nextBlock) {
            printf("Previous and next pointers within block[%p] in list[%p] are not consistent. \n", _nextBlock, listPtr);
        }
        if (GET(HDRP(_nextBlock)) != GET(FTRP(_nextBlock))){
            printf("Header and footer mismaches, within block[%p] in list[%p]. \n ",_nextBlock, listPtr);
        }
        if(GET_SIZE(HDRP(_nextBlock))<DSIZE){
            printf("Block[%p] in list[%p] is too small. \n", _nextBlock, listPtr);
        }
        if (GET_SIZE(HDRP(_nextBlock))>max || GET_SIZE(HDRP(_nextBlock))<min) {
            printf("Out of range, Block[%p] should not be in list[%p]. \n", _nextBlock, listPtr);
        }
        _nextBlock = NEXT_FREE_P(_nextBlock);
    }
    
}

/*
 * checkEachBlockInPayload - check block in heap one by one, dealing with
 * boundaries, header/footer consistency, coalescing, alignment, etc.
 */
void checkEachBlockInPayload(void *payloadPtr){
    char *bp = payloadPtr;
    if(!in_heap(bp)) {
        printf("Block[%p] is out of heap (%p, %p)\n",bp, mem_heap_lo(), mem_heap_hi());
    }
    if (!ALIGNED(bp)) {
        printf("Align issue within block[%p]. \n", bp);
    }
    if (GET_ALLOC(HDRP(bp))&&GET_PREV_ALLOC(HDRP(bp))){
        printf("Coalesce should have happend for block[%p] and block[%p]. \n", PREV_BLKP(bp),bp);
    }
    if (GET_PREV_ALLOC(HDRP(bp))!=GET_ALLOC(HDRP(PREV_BLKP(bp)))){
        printf("The alloc bits of Block[%p] and its previous do not match. \n", bp);
    }
    
    
    
}






