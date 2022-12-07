#include "filesys/file.h"
#include "filesys/off_t.h"
#include "threads/palloc.h"
#include <list.h>
#include <hash.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct bitmap *swap_table;
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;

struct spt_hash{
    struct hash spt_hash;
};

struct page{
    struct hash_elem elem;
    void *vaddr;
    struct frame *frame_by_page;
    struct spt_data *data;
    union {
    struct uninit_page uninit;
    struct anon_page anon;
    struct file_page file;
    }

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

struct container{
    struct file* file;
    off_t offset;
    size_t page_read_bytes;
}

void frame_init();

struct page* spt_find(struct hash* h, void *addr);
struct page* spt_add(struct hash* h, struct page *p);
struct hash_elem* spt_del(struct hash* h, struct page *p);

struct frame* get_frame(struct page* p, enum palloc_flags pf);
void frame_free(struct frame* f);

struct spt_data* make_spt_data(struct file* f, off_t o, uint32_t r, bool w);
bool lazy_load(struct hash *h, void* addr);

void* do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset);
void* do_munmap(void *addr);

bool pagedir_is_dirty (uint32_t *pagedir, const void *vpage);
void pagedir_set_dirty (uint32_t *pagedir, const void *vpage, bool dirty);
void pagedir_clear_page (uint32_t *pagedir, void *upage);

void vm_anon_init (void);
static bool anon_swap_out (struct page *page);
static bool anon_swap_in (struct page *page, void *kva);