#include <stdio.h>
#include <stdlib.h>

int main() {
  void *head_f2 = malloc(0x10000);
  void *head_f = malloc(0x10000);
  void *a = malloc(0x10000);
  void *b = malloc(0x10000);
  void *c = malloc(0x10000);
  void *d = malloc(0x10000);
  void *e = malloc(0x10000);
  void *tail_f = malloc(0x10000);
  void *tail_f2 = malloc(0x10000);

  free(head_f);
  free(tail_f);
  free(b);
  free(d);
  free(c);
  free(tail_f2);
  free(head_f2);

  void *f = malloc(0x26000);
  void *g = malloc(1);
}