/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
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
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
/*64

/* 32비트 운영체제를 사용하기 때문에 8바이트 사용...*/
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
/* size를 8의 배수로 올림 연산을 해주는 것이다..*/
/* (size + 7) : 8로 나누어 떨어지게 만들기 위해 +7
~0x7 = 0xFFFFFFF8 (하위 3비트 0 → AND 하면 하위 비트 날리고 8의 배수 됨))
비트 마스킹을 활용한 빠른 나머지 제거 방식 */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/* size_t 크기를 8의 배수로 올린 값 (정렬 안전성 확보) */
/*정렬 깨지면 성능 문제 + 크래시 위험*/
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* Basic constants and macros */
#define WSIZE 4 /* word size (byte)*/
#define DSIZE 8 /* double word SIZE (byte) */
#define CHUNKSIZE (1<<12) /* 초기 가용 블록과 힙 확장을 위한 크기 (byte)*/

#define MAX(x, y) ((x) > (y) ? (x) : (y)) // 둘중 더 큰 값을 반환하는 max 매크로

/*Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) /* 헤더, 푸터를 만들기 위한 매크로*/

/*Read and write a word at address p */
/*여기서 p는 메모리블록의 헤더 푸터를 가르킨다.*/
/*헤더와 푸터를 조작하는 매크로*/
#define GET(p)      (*(unsigned int *)(p)) // 해당 주소에 저장된 4바이트의 값을 읽어오는 것
#define PUT(p, val) (*(unsigned *)(p) = (val)) //해당 주소에 4바이트의 값을 써주는 것

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7) // 하위 3비트 무시 (사이즈만)
#define GET_ALLOC(p)    (GET(p) & 0X1) // 하위 1비트에 1을 넣고 and연산 (할당 여부만)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE) // payload 기준 4바이트(1워드) 앞으로 가서 헤더 찾기
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)// payload 기준 블록크기만큼 뒤로 가서 푸터 찾기

/* Given bloack ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/* 명시적 가용리스트 관리를 위한 매크로 추가 */

// * payload 안에 pred 와 succ 를 찾는 매크로
#define SUCC(bp) (*(void **)(bp)) // payload 시작주소 (즉, pred 시작주소) 
#define PREP(bp) (*(void **)((char *)(bp) + WSIZE))// pred 다음주소 (즉 succ 시작주소)


static void * free_listp; // *  명시적 가용 리스트 관리를 위한 listp
static void * heap_listp;
// * next_fit 사용시 last_bp 사용
static void * last_bp;
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
static void place(void *, size_t);

/* 명시적 가용 리스트 관리를 위한 함수*/
static void new_free_block(void *);
static void remove(void *);


/*
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    /* Create the initial empty heap*/
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0); // [0]: Padding (더미 4바이트)
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE*2, 1)); // [4]: Prologue Header (16B, allocated)
    PUT(heap_listp + (2*WSIZE), (int)NULL); // [8]: Prologue SUCCESSOR  (4B) -> NULL
    PUT(heap_listp + (3*WSIZE), (int)NULL); // [12]: Prologue PREDECESSOR  (4B) -> NULL
    PUT(heap_listp + (4*WSIZE), PACK(DSIZE*2, 1)); // [16]: Prologue Footer (16B, allocated)
    PUT(heap_listp + (5*WSIZE), PACK(0, 1)); // [20]: Epilogue Header (0B, allocated)
    
    free_listp = heap_listp + DSIZE;

    /* Extend the empty heap with a free block of CHUNKESIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    /* Coalesce if the previous block was free*/
    return coalesce(bp);
}

// 새로 반환되거나 생성된 가용 블록을 가용 리스트에 추가
// * 이때 반환되는건 항상 루트의 다음 노드가 된다.
static void new_free_block(void *bp){
    PREP(bp) = NULL;
    SUCC(bp) = free_listp;
    PREP(free_listp) = bp;
    free_listp = bp;
}

static void remove(void *bp){ 
// 생각 해야 할 껀 단 두개 
// 1. 삭제할 bp 가 head 일때
// 2. head 아닐때 (양 옆에 둘다 있을 때)

// 양 옆에 한개가 있을 때는 상황이 안나옴...
// 설정해둔 프롤로그 블록이 뒤를 항상 지키는 중 !

    if (bp == free_listp){ // * 삭제할 bp가 head일때
        PREP(SUCC(bp)) = NULL;
        free_listp = SUCC(bp);
    }

    else{ // * 양 옆에 둘다 있을 때
        PREP(SUCC(bp)) = PREP(bp);
        SUCC(PREP(bp)) = SUCC(bp);
    }
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if(prev_alloc && next_alloc){ // * case 1
        // *next_fit 사용시 추가
        last_bp = bp;
        return bp;
    }

    else if(prev_alloc && !next_alloc) { // *case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if(!prev_alloc && next_alloc){ // *case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else { // * case 4
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // *next_fit 사용시 추가
    last_bp = bp;
    return bp;
}


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if(size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE -1)) / DSIZE);

    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize){

    // *first_fit

    // void *bp;

    // for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
    //         return bp;
    //     }
    // }
    // return NULL; 

    // * next_fit

    void *bp = last_bp;

    // last_bp부터 힙 끝까지 탐색
    for(bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            last_bp = bp; // 찾았으면 last_bp 업데이트
            return bp;
        }
    }

    bp = heap_listp;
    // 못 찾으면 heap_listp부터 last_bp까지 다시 탐색
    while (bp < last_bp) {
        bp = NEXT_BLKP(bp);
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            last_bp = bp;
            return bp;
        }
    }

    return NULL;

    // // * best_fit

    // void *bp;
    // void *best_bp = NULL;
    // size_t best_size = (size_t)(-1); // unsignded int 특성상 음수 값이 해당 자료형의 최댓값
    // size_t size;

    // for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0 ; bp = NEXT_BLKP(bp)){
    //     size = GET_SIZE(HDRP(bp));

    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= size)){
    //         if(size < best_size){
    //             best_size = size;
    //             best_bp = bp;
    //         }
    //     }
    // }

    // return best_bp;
}

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }

    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}