#include <sys/types.h>
#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
struct pageHeader { // 10 bytes. 8 for page_t, 1 for u_int8_t
  u_int8_t size;
  u_int8_t nextFree;
  struct page_t *nextPage;
};
int main(void) {
  // int index = log2(1);
  // printf("%d\n", index);
  // int* test = (int*) malloc(2);
  // *test = 100;
  // printf("test:  %d\n", *test);
  // int* test2 = (int*) malloc(2);
  // *test2 = 200;
  // printf("test2:  %d\n", *test2);

  // printf("zero: %d\n", open("/dev/zero", O_RDWR));
  int test1AnswerKey = 0;
  // int test1Answer;
  int *test1[20000];
  for (size_t i = 0; i < 20000; i++) {
    test1[i] = (int *)malloc(16);
    // *test1[i] = 200 + i;
    memset(test1[i], 0, 16);
    // test1Answer = *test1[i]
    assert(*test1[i] == test1AnswerKey);
    // printf("test1[%ld]:  %d\n", i, *test1[i]);
  }
  printf("malloc 16 checks PASS \n");

  // int test2AnswerKey = (1 << 17) -1;
  int *test2[10000];
  for (size_t i = 0; i < 1000; i++) {
    test2[i] = (int *)malloc(1024);
    memset(test2[i], 1, 1024);
    // *test1[i] = 200 + i;
    // printf("test2[%ld]:  %d\n", i, *test2[i]);
    assert(*test2[i] == 16843009);
    free(test2[i]);
  }
  printf("malloc and free 1024 checks PASS \n");

  int test3AnswerKey = 1 << 1030;
  int *test3 = (int *)malloc(1030);
  memset(test3, 1, 1030);
  printf("large alloc: %d\n", *test3);
  // assert(*test3 == test3AnswerKey);
  // printf("One large alloc check passes");

  u_int64_t reallocTestKey = (1 << 16) - 1;
  int *reallocTest = (int *)malloc(2);
  memset(reallocTest, 0b11111111, 2);
  // printf("reallocTest: %d\n", *reallocTest);
  int *realloc2 = (int *)realloc(reallocTest, 8);
  // printf("realloc2: %d\n", *realloc2);
  assert(*realloc2 == reallocTestKey);
  printf("Realloc from small to large alloc PASS\n");

  // u_int8_t reallocTest2Key = 0b01101010;
  // u_int16_t reallocTest2Key2 = 0b1010101010101010;
  u_int32_t reallocTest2Key = 0x55555555;
  int *reallocTest2 = (int *)malloc(4000);
  memset(reallocTest2, 0x55, 4000);
  for (int i = 0; i < 4000 / sizeof(int); i++) {
    assert(*(reallocTest2 + i) == 0x55555555);
  }
  int *largerReallocTest2 = (int *)realloc(reallocTest2, 4712);
  for (int i = 0; i < 4000 / sizeof(int); i++) {
    assert(*(largerReallocTest2 + i) == 0x55555555);
  }
  for (int i = 4000 / sizeof(int); i < (4712 - 4000) / sizeof(char); i++) {
    assert(*((char *)(largerReallocTest2 + i)) != 0x55);
  }
  // int * realloc3 = (int*) realloc(reallocTest2, 1);
  // assert(reallocTest2Key == *realloc3);
  printf("Realloc from large to small alloc PASS\n");
  // printf("reallocTest2: %d\n", *reallocTest2);
}
