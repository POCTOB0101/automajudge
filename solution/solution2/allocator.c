#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// #define TL_OPTIMIZED

#define PAGE_SIZE 4096
#define HEADERS_SIZE 22
#define SMALL_BLOCK_SIZES 10
#define MAX_SMALL_BLOCK_SIZE 1024

typedef struct node {
  struct node *next;
} node;

typedef struct page {
  uint16_t size;
  node *freelist;
  struct page *nextpage;
} page;

pthread_mutex_t allocator_lock = PTHREAD_MUTEX_INITIALIZER;

#define FIRSTNODE(PAGE) (((uint8_t *)PAGE) + sizeof(struct node))
#define NODESIZE(size) (size + sizeof(struct node))
#define GETBLOCKN(PAGE, SIZE, N)                                               \
  ((node *)((((uint8_t *)PAGE) + sizeof(page)) + (NODESIZE(SIZE) * (N))))
#define GET_NODE_DATA(PTR) (((uint8_t *)PTR) + sizeof(struct node))
#define GET_NODE_FROM_DATA(DATAPTR)                                            \
  ((node *)(((uint8_t *)DATAPTR) - sizeof(struct node)))
#define GETOFFSET(PTR) (((uintptr_t)PTR) & 0x0000000000000FFFULL)
#define GETPAGEPTR(PTR) ((page *)(((uintptr_t)PTR) & 0xFFFFFFFFFFFFF000ULL))

#ifdef TL_OPTIMIZED
thread_local struct page *smallPages[SMALL_BLOCK_SIZES] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
#else
struct page *smallPages[SMALL_BLOCK_SIZES] = {NULL, NULL, NULL, NULL, NULL,
                                              NULL, NULL, NULL, NULL, NULL};
#endif

uint16_t get_block_size_log2(uint32_t size);

// for small blocks only.
inline uint16_t get_block_size_log2(uint32_t size) {
  assert(size <= MAX_SMALL_BLOCK_SIZE);

  if (size < 2)
    size = 2;

  uint16_t size_log2 = ((sizeof(size) * CHAR_BIT)) - __builtin_clz(size - 1);
  return size_log2;
}

void *map_large_block(uint32_t size) {
  uint32_t mapsize = size + sizeof(uint32_t);
  uint8_t *rawblock = mmap(NULL, mapsize, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (rawblock == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  *(uint32_t *)rawblock = mapsize;
  void *result = rawblock + sizeof(uint32_t);
  return result;
}

void *map_small_block_page(uint32_t size) {
  page *newpage = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (newpage == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  newpage->freelist = (node *)(((uint8_t *)newpage) + sizeof(page));
  newpage->nextpage = NULL;
  newpage->size = size;

  uint32_t numnodes = (PAGE_SIZE - (sizeof(struct page))) / (NODESIZE(size));

  for (int i = 0; i < numnodes - 2; i++) {
    node *current = GETBLOCKN(newpage, size, i);
    current->next = (node *)(((uint8_t *)current) +
                             NODESIZE(size)); // GETBLOCKN(newpage, size, i+1);
  }
  node *current = GETBLOCKN(newpage, size, numnodes - 1);
  current->next = NULL;
  return newpage;
}

void *malloc(size_t size) {
  if (size == 0)
    return NULL;

  if (size > MAX_SMALL_BLOCK_SIZE) {
    return map_large_block(size);
  }

  // small block

#ifndef TL_OPTIMIZED
  pthread_mutex_lock(&allocator_lock);
#endif

  uint32_t blocksize_log2 = get_block_size_log2(size);
  uint32_t blocksize = 1 << blocksize_log2;
  page *sizelist = smallPages[blocksize_log2 - 1]; // no list for 1-byte blocks

  page *current = sizelist;
  while (current) {
    if (current->freelist) {
      // allocate it
      void *result = GET_NODE_DATA(current->freelist);
      current->freelist = current->freelist->next;

#ifndef TL_OPTIMIZED
      pthread_mutex_unlock(&allocator_lock);
#endif
      return result;
    }
    current = current->nextpage;
  }

  // no free blocks - map more
  page *newpage = map_small_block_page(blocksize);
  newpage->nextpage = sizelist;
  smallPages[blocksize_log2 - 1] = newpage;

  void *result = GET_NODE_DATA(newpage->freelist);
  newpage->freelist = newpage->freelist->next;

#ifndef TL_OPTIMIZED
  pthread_mutex_unlock(&allocator_lock);
#endif
  return result;
}

void *calloc(size_t numEl, size_t elementSize) {
  size_t size = numEl * elementSize;

  void *mem = malloc(size);
  memset(mem, 0, size);
  return mem;
}

bool is_large_block(void *ptr) { return (GETOFFSET(ptr) == sizeof(uint32_t)); }

size_t get_size_of_block(void *ptr) {
  page *pp = GETPAGEPTR(ptr);
  if (is_large_block(ptr)) {
    return (*((uint32_t *)pp)) - sizeof(uint32_t);
  }
  assert(pp->size > 0 && pp->size <= MAX_SMALL_BLOCK_SIZE);
  return pp->size;
}

void *realloc(void *ptr, size_t size) {

  if (ptr == NULL)
    return malloc(size);

  if (size == 0) {
    free(ptr);
    return NULL;
  }

  size_t orig_size = get_size_of_block(ptr);
  size_t new_size = size;

  if (size <= MAX_SMALL_BLOCK_SIZE) {
    uint32_t blocksize_log2 = get_block_size_log2(size);
    new_size = 1 << blocksize_log2;
  }

  if (orig_size == new_size)
    return ptr;

  size_t copy_size = (orig_size < new_size ? orig_size : new_size);

  void *mem = malloc(size);
  memcpy(mem, ptr, copy_size);
  free(ptr);
  return mem;
}

void free(void *obj) {
  if (obj == NULL) {
    return;
  }

  page *pp = GETPAGEPTR(obj);

  if (is_large_block(obj)) {
    size_t mapsize = *((uint32_t *)pp);
    munmap(pp, mapsize);
    return;
  }

// small blocks
#ifndef TL_OPTIMIZED
  pthread_mutex_lock(&allocator_lock);
#endif

  node *n = GET_NODE_FROM_DATA(obj);
  n->next = pp->freelist;
  pp->freelist = n;

#ifndef TL_OPTIMIZED
  pthread_mutex_unlock(&allocator_lock);
#endif
  return;
}
