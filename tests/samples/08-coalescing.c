#include <stdio.h>
#include <stdlib.h>

int main() {
  void *a = malloc(0x10000);
  void *b = malloc(0x10000);
  void *c = malloc(0x10000);
  void *d = malloc(0x10000);
  void *e = malloc(0x10000);

  free(b);
  free(c);
  free(d);

  void *f = malloc(0x26000);
  void *g = malloc(1);
}