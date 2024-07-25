#include "tester-utils.h"

int main() {
  // Test if realloc with 0 returns NULL ptr;
  void *ptr1 = malloc(1000);
  void *ptr1_new = realloc(ptr1, 0);
  if ( ptr1_new != NULL) {
    return 1;
  }

  return 0;
}
