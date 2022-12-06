#include "vm/vm.h"
#include "userprog/pagedir.h"
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/synch.h"

struct list frame_table;
struct lock frame_lock;

struct page* spt_find(struct hash* h, void *addr)
{
    if(!is_user_vaddr(addr))
        return NULL;
    struct thread* cur=thread_current();
    struct page p;
    p.vaddr=pg_round_down(addr);
    struct hash_elem* e=hash_find(cur->spt, &p.page_elem);
    if(e==NULL)
        return NULL;
    return hash_entry(e, struct page, page_elem);
}

struct page* spt_add(struct hash* h, struct page* p)
{
    hash_insert(h, &p->page_elem);
    return NULL;
}

struct page* spt_del(struct hash* h, struct page *p)
{
    struct hash_elem *e=hash_delete(h, &p->page_elem);
    return e;
}

struct frame* get_frame(struct page* p)
{
    lock_acquire(&frame_lock);
    struct frame *f =malloc(sizeof(struct frame));
    void* addr=palloc_get_page(PAL_USER);
    if(addr==NULL)
        //swap_out();
        return NULL;
    else{
        f->addr=addr;
        f->page=p;
    }
    list_insert(&frame_table, &f->frame_elem);
    lock_release(&frame_lock);
    return f;
}

void frame_free(struct frame* f)
{
    lock_acquire(&frame_lock);
    list_remove(&f->frame_elem);
    palloc_free_page(f->addr);
    free(f);
    lock_release(&frame_lock);
}

struct spt_data* make_spt_data(struct file* f, off_t o, uint32_t r, bool w)
{
    struct spt_data* data=malloc(sizeof(struct spt_data));
    data->file=f;
    data->ofs=o;
    data->read_bytes=r;
    data->writable_flag=w;

    return data;
}

bool lazy_load(struct hash *h, void* addr)
{
    struct thread* t=thread_current();
    struct page *p = spt_find(h, addr);
    if(p==NULL)
        return false;
    if(p->frame!=NULL)
        return true;

    struct frame* f=get_frame(p);
    p->frame=f;
    struct spt_data* d=p->data;

    file_seek (d->file, d->ofs);
    if (file_read (d->file, p->frame->addr, d->read_bytes) != (int) d->read_bytes)
        {
          frame_free(f);
          p->frame=NULL;
          return false;
        }
      memset (p->frame->addr + d->read_bytes, 0, PGSIZE - d->read_bytes);

     if(!pagedir_set_page (t->pagedir, addr, f->addr, p->data->writable_flag))
        {
            frame_free(f);
            p->frame=NULL;
            return false;
        }
    return true;
}