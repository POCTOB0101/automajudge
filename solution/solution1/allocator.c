#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096

#define HEADERS_SIZE 22

void __attribute__((constructor)) lib_init();

struct pageHeader { // 16 bytes. 8 for page_t, 2 for u_int16 _t
  u_int16_t size;
  u_int16_t freelist; // offset for the next free block for when there are no
                      // prior free blocks.
  struct pageHeader *nextPage;
};

struct metadata { // 6 bytes
  u_int16_t size;
  bool isFree;
  u_int16_t nextFree;
};

struct pageHeader *smallPages[10];
int fd;

void lib_init() {
  fd = open("/dev/zero", O_RDWR);
  for (int i = 0; i < 10; i++) {
    smallPages[i] = NULL;
  }
}

void *malloc(size_t size) {
  if (size == 0) {
    return NULL;
  }
  float twoPower = log2(size);
  int index = (int)twoPower; // gets the index for the smallPages array
  int sizeGroup = pow(2, index + 1);

  void *mem;
  if (twoPower <= 10.00) {
    struct pageHeader *page;
    if (smallPages[index] == NULL) { // first allocation of that size group
      smallPages[index] =
          (struct pageHeader *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);
      smallPages[index]->size = sizeGroup;
      smallPages[index]->nextPage = NULL;
      smallPages[index]->freelist = sizeof(struct pageHeader);
      page = smallPages[index];
    } else {
      page = smallPages[index];
      while (page->nextPage != NULL) {
        page = page->nextPage;
      }
      if (page->freelist + sizeof(struct metadata) + sizeGroup >
          PAGE_SIZE) { // if the next free space is too small for the allocation
        page->nextPage =
            (struct pageHeader *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);
        page = page->nextPage;
        page->size = sizeGroup;
        page->nextPage = NULL;
        page->freelist = sizeof(struct pageHeader);
      }
    }
    struct metadata *meta =
        (struct metadata *)(((char *)page) + page->freelist);
    meta->isFree = false;
    meta->size = sizeGroup;
    meta->nextFree = 0; // set the groundwork for the free list
    mem = (void *)(meta + 1);
    page->freelist += (sizeof(struct metadata) + sizeGroup);
  } else { // larger allocation than 2^10 (1024)
    int pageCount = ceil((double)sizeGroup /
                         PAGE_SIZE); // to make sure the page is big enough and
                                     // is a multiple of PAGE_SIZE
    struct pageHeader *page = (struct pageHeader *)mmap(
        NULL, pageCount * PAGE_SIZE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);
    page->size = pageCount * PAGE_SIZE;
    page->nextPage = NULL;
    page->freelist = sizeof(struct pageHeader);
    struct metadata *meta =
        (struct metadata *)(((char *)page) + sizeof(struct pageHeader));
    meta->isFree = false;
    meta->size = page->size;
    meta->nextFree = 0; // set the groundwork for the free list
    mem = (void *)(meta + 1);
    page->freelist += (sizeof(struct metadata) + size);
  }

  return mem;
}

void *calloc(size_t numEl, size_t elementSize) {
  int size = numEl * elementSize;
  if (size < numEl) { // checks for int overflow
    return NULL;
  }
  void *mem = malloc(size);
  memset(mem, 0, size);
  return mem;
}

void *realloc(void *ptr, size_t size) {
  void *mem = malloc(size);
  struct metadata *meta = (struct metadata *)ptr - 1;
  memcpy(mem, ptr,
         (size < meta->size)
             ? size
             : meta->size); // copies eithre the size of the new allocation or
                            // the old allocation, whichever is smallest
  free(ptr);
  return mem;
}

void free(void *obj) {
  if (obj == NULL) {
    return;
  }

  struct metadata *meta = (struct metadata *)obj - 1;
  if (meta->size < PAGE_SIZE) { // smaller allocations
    meta->isFree = true;
    // here is where meta->nextFree would be set and page->nextFree for the free
    // list, but my implementation broke things,
    //  meta->nextFree
  } else { // larger allocations
    struct pageHeader *page = ((struct pageHeader *)meta) - 1;
    munmap((void *)page, page->size);
  }
  return;
}
