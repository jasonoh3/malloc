#include <stdio.h>
#include <stdlib.h>

#define NUM_TIMES 100000

int main() {
  printf("-- Allocation --\n");
  for (int i = 0; i < NUM_TIMES; i++) {
    malloc(100);    
  }
  void *ptr = malloc(100);
  for (int i = 0; i < NUM_TIMES; i++) {
    malloc(100);
  }

  printf("-- Free + Re-allocate --\n");
  for (int i = 0; i < NUM_TIMES; i++) {
    free(ptr);
    ptr = malloc(100);
  }
}