#include "filesys/file.h"
#include "filesys/off_t.h"
#include <list.h>
#include <hash.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct spt_hash{
    struct hash spt_hash;
};

struct page{
    struct hash_elem elem;
    void *vaddr;
    struct frame *frame_by_page;
    struct spt_data *data;
};

struct frame{
    void *addr;
    struct page *page_of_frame;
    struct list_elem frame_elem;
};

struct spt_data{
    struct file* file;
    off_t ofs;
    uint32_t read_bytes;
    bool writable_flag;
};

void frame_init();

struct page* spt_find(struct hash* h, void *addr);
struct page* spt_add(struct hash* h, struct page *p);
struct hash_elem* spt_del(struct hash* h, struct page *p);

struct frame* get_frame(struct page* p);
void frame_free(struct frame* f);

struct spt_data* make_spt_data(struct file* f, off_t o, uint32_t r, bool w);
bool lazy_load(struct hash *h, void* addr);