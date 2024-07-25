#include "tester-utils.h"

int main() {
  char TEST_STRING[] = "Hello world!";

  char *a = malloc(0x10000);
  char *b = malloc(0x1000);
  char *c = malloc(0x100);
  char *d = malloc(0x1000);
  char *e = malloc(0x10000);

  strcpy(c, TEST_STRING);

  char *c2 = realloc(c, 0x10000);

  if (strcmp(c2, TEST_STRING) != 0) {
    return 1;
  }

  return 0;
}
