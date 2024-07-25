#include <stdio.h>
#include <stdlib.h>

int main() {
  void *a = malloc(0x10000);
  void *b = malloc(0x10000);
  void *c = malloc(0x10000);
  void *d = malloc(0x10000);
  void *e = malloc(0x10000);

  free(d);
  free(c);
  free(b);

  // Some of these blocks fit within free memory, but not all of them:
  void *f = malloc(0x5000);
  void *g = malloc(0x20000);
  void *h = malloc(0x5000);
  void *i = malloc(0x20000);
  void *j = malloc(0x5000);
}