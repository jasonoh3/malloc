#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>

static void *sbrk(intptr_t increment) {
  static void *sbrk_mmap = NULL;
  static intptr_t sbrk_offset = 0;

  if (!sbrk_mmap) {
    sbrk_mmap = (char*)mmap(NULL, 2L * 1024 * 1024 * 1024, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
  }

  sbrk_offset += increment;
  return sbrk_mmap + sbrk_offset;
}
