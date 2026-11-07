#include <stddef.h>
// Use ESP-IDF's standard FAT VFS. No Arduino virtual filesystem or borrowed flash pointers.
void *espeak_mem_map(const char *path, int *length) {
    (void)path; (void)length; return NULL;
}
