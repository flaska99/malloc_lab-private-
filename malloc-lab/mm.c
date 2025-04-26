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
/* size를 8의 배수로 올림 연산을 해주는 것이다.*/
/* (size + 7) : 8로 나누어 떨어지게 만들기 위해 +7
~0x7 = 0xFFFFFFF8 (하위 3비트 0 → AND 하면 하위 비트 날리고 8의 배수 됨)
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


static void * heap_listp;
static void *extend_heap(size_t words);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap*/
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0); // [0]: Padding (더미 4바이트)
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // [4]: Prologue Header (8B, allocated)
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // [8]: Prologue Footer (8B, allocated)
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // [12]: Epilogue Header (0B, allocated)
    heap_listp += (2*WSIZE);

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


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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