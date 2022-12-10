#include "filesys/file.h"
#include "lib/kernel/list.h"

struct mmap_file{
    int mapid;
    struct file* file;
    struct list_elem elem;
    struct list vme_list;
}

