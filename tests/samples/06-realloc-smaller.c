#include "tester-utils.h"

int main() {
  // If I am reducing size, it should use the same block?
  void *ptr2 = malloc(0x10000);
  void *ptr3 = realloc(ptr2, 0x8000);
  void *ptr4 = realloc(ptr3, 0x1000);
  void *ptr5 = realloc(ptr4, 0x800);
  void *ptr6 = realloc(ptr5, 0x100);

  return 0;
}
