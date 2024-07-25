#include <stdlib.h>

int main() {
  const int TEST_SIZE = 10240;
  char *buffer = malloc(TEST_SIZE);

  int i;
  for (i = 0; i < TEST_SIZE; i++) {
    *buffer = 'a';
  }

  free(buffer);
  return 0;
}
