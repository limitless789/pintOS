#include "filesys/file.h"
#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

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
    struct list_elem frame_elem;
};

struct spt_data{
    struct file* file;
    off_t ofs;
    uint32_t read_bytes;
    bool writable_flag;
};


struct page* addr2pg(void *addr);