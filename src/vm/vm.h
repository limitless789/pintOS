#include "filesys/file.h"
#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct page{
    struct hash_elem page_elem;
    void *vaddr;
    struct frame *frame;
    struct spt_data *data;
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

struct page* spt_find(struct hash* h, void *addr);
struct page* spt_add(struct hash* h, struct page *p);
struct page* spt_del(struct hash* h, struct page *p);

struct frame* get_frame(struct page* p);
void frame_free(struct frame* f);

struct spt_data* make_spt_data(struct file* f, off_t o, uint32_t r, bool w);
bool lazy_load(struct hash *h, void* addr);