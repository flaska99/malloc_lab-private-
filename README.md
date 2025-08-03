# 🧠 Malloc Lab - Dynamic Memory Allocator

> **카네기 멜론 대학 CS:APP (Computer Systems: A Programmer's Perspective) 과제**  
> C언어로 malloc, free, realloc 함수를 직접 구현하여 동적 메모리 할당기를 제작

## 📋 프로젝트 개요

이 프로젝트는 카네기 멜론 대학의 Malloc Lab 과제를 통해 동적 메모리 할당기를 단계적으로 구현한 학습 기록입니다. 묵시적 가용 리스트부터 명시적 가용 리스트까지 다양한 메모리 할당 전략을 구현하고 성능을 최적화했습니다.

### 🎯 학습 목표
- 동적 메모리 할당의 내부 동작 원리 이해
- C언어의 포인터와 메모리 구조에 대한 심화 학습
- 다양한 할당 알고리즘의 성능 분석 및 비교
- 메모리 단편화 문제 해결 방안 탐구

## 🏗️ 프로젝트 구조

```
malloc_lab/
├── mm.c                    # 메인 구현 파일
├── mm.h                    # 헤더 파일
├── mdriver.c              # 테스트 드라이버
├── memlib.c               # 힙 시뮬레이터
└── traces/                # 테스트 케이스들
```

## 📚 학습한 핵심 개념

### 1. 동적 메모리 할당 기초
- **힙(Heap) 영역**: 프로세스의 가상 메모리에서 동적 할당을 위한 영역
- **브레이크 포인터(brk)**: 힙의 꼭대기를 가리키는 포인터
- **블록 구조**: 헤더, 페이로드, 푸터로 구성된 메모리 블록

### 2. 메모리 단편화
- **내부 단편화**: 할당된 블록 내에서 사용하지 않는 공간
- **외부 단편화**: 가용 블록들이 작게 흩어져 큰 요청을 처리할 수 없는 상황
- **연결(Coalescing)**: 인접한 가용 블록들을 합치는 기법

### 3. 경계 태그(Boundary Tags)
```c
// 블록 구조
[헤더(4B)][페이로드][푸터(4B)]
// 헤더/푸터: 블록 크기 + 할당 상태 비트
```

## 🔧 구현한 할당기 유형

### 1. 묵시적 가용 리스트 (Implicit Free List)

가장 기본적인 형태로, 모든 블록을 순차적으로 탐색하여 가용 블록을 찾는 방식입니다.

**주요 특징:**
- 선형 탐색으로 가용 블록 검색
- 구현이 단순하지만 성능이 제한적
- First-fit, Next-fit, Best-fit 알고리즘 비교 구현

**성능 결과:**
- Next-fit: 가장 높은 처리량 달성
- Best-fit: 메모리 효율성은 좋으나 속도 저하
- First-fit: 균형잡힌 성능

### 2. 명시적 가용 리스트 (Explicit Free List)

가용 블록들만을 연결 리스트로 관리하여 탐색 효율성을 크게 향상시킨 방식입니다.

**주요 특징:**
- 가용 블록의 페이로드에 이전/다음 포인터 저장
- 가용 블록만 탐색하여 시간 복잡도 개선
- 링크드 리스트 연산으로 동적 관리

**핵심 구현:**
```c
#define PREV(bp) (*(void**)(bp))
#define NEXT(bp) (*(void**)(bp + WSIZE))

// 가용 리스트에 블록 추가
void putFreeBlock(void *bp) {
    NEXT(bp) = free_listp;
    PREV(bp) = NULL;
    PREV(free_listp) = bp;
    free_listp = bp;
}

// 가용 리스트에서 블록 제거
void removeBlock(void *bp) {
    if(bp == free_listp) {
        PREV(NEXT(bp)) = NULL;
        free_listp = NEXT(bp);
    } else {
        NEXT(PREV(bp)) = NEXT(bp);
        PREV(NEXT(bp)) = PREV(bp);
    }
}
```

**성능 개선:**
- 묵시적 리스트 대비 탐색 속도 대폭 향상
- Best-fit 알고리즘에서 최고 성능 달성

## ⚡ 성능 최적화

### 1. 연결(Coalescing) 최적화
4가지 경우에 대한 체계적인 블록 병합 구현:
- Case 1: 양쪽 모두 할당된 상태
- Case 2: 다음 블록만 가용 상태
- Case 3: 이전 블록만 가용 상태  
- Case 4: 양쪽 모두 가용 상태

### 2. 배치(Placement) 전략
- 요청 크기에 맞는 블록 분할
- 최소 블록 크기 보장
- 내부 단편화 최소화

### 3. 할당 정책 비교
각 할당 정책의 특성을 실험을 통해 분석:
- **처리량(Throughput)**: 단위 시간당 처리 가능한 요청 수
- **메모리 이용률**: 실제 사용 중인 메모리 비율

## 🛠️ 핵심 함수 구현

### malloc 구현
```c
void *mm_malloc(size_t size) {
    size_t asize;      // 정렬된 블록 크기
    size_t extendsize; // 힙 확장 크기
    char *bp;

    if (size == 0) return NULL;
    
    // 크기 정렬 (8바이트 단위)
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // 적합한 가용 블록 검색
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 가용 블록이 없으면 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    
    place(bp, asize);
    return bp;
}
```

### free 구현
```c
void mm_free(void *ptr) {
    if (ptr == NULL) return;
    
    size_t size = GET_SIZE(HDRP(ptr));
    
    // 헤더와 푸터를 가용 상태로 변경
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    
    // 인접 블록과 병합
    coalesce(ptr);
}
```

## 📊 성능 분석 결과

### 명시적 가용 리스트 + Best-fit
- **메모리 이용률**: 높은 효율성
- **처리량**: 우수한 성능
- **전체 점수**: 목표 달성

### 학습한 최적화 기법
1. **알고리즘 선택의 중요성**: Best-fit이 명시적 리스트에서 최적
2. **자료구조의 영향**: 링크드 리스트를 통한 탐색 시간 단축
3. **메모리 정렬**: 8바이트 정렬을 통한 성능 향상

## 🎓 배운 점과 성찰

### 기술적 학습
- **포인터 조작의 숙련도 향상**: 복잡한 포인터 연산과 메모리 구조 이해
- **시스템 프로그래밍 감각**: 하드웨어 수준의 메모리 관리 원리 체득
- **알고리즘 최적화**: 이론과 실제 성능의 차이 경험

### 문제 해결 능력
- **단계적 접근**: 묵시적 → 명시적 리스트로 점진적 개선
- **디버깅 스킷**: 복잡한 메모리 버그 추적 및 해결
- **성능 분석**: 다양한 메트릭을 통한 객관적 평가

---
**참고 자료**: CS:APP (Computer Systems: A Programmer's Perspective)

> 이 프로젝트를 통해 메모리 관리의 깊이 있는 이해를 얻었으며, 시스템 프로그래밍 역량을 크게 향상시킬 수 있었습니다.
