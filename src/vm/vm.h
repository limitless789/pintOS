
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct page{
    struct hash_elem hash_elem;
    void *vaddr;
    bool ronly;
    struct frame *frame;
};

struct frame{
    void *addr;
    struct page *page;
};

struct page* addr2pg(void *addr);