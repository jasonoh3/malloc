#include "tester-utils.h"

int main() {
    for (int i = 0; i < 10; i++) {
        int *ptr = calloc(i+1, sizeof(int));
        verify_clean((char *)ptr, (i+1)*sizeof(int));

        if (ptr == NULL) {
            fprintf(stderr, "Memory failed to allocate!\n");
            return 1;
        }

        *ptr = 4;
        free(ptr);
    }

    for (int i = 0; i < 10; i++) {
        int *ptr = calloc((10 - i)+1, sizeof(int));
        verify_clean((char *)ptr, ((10 - i)+1)*sizeof(int));

        if (ptr == NULL) {
            fprintf(stderr, "Memory failed to allocate!\n");
            return 1;
        }

        *ptr = 4;
        free(ptr);
    }

    fprintf(stderr, "Memory was allocated, used, and freed!\n");
    return 0;
}
