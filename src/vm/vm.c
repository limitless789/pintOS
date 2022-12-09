#include "vm/vm.h"
#include "userprog/pagedir.h"
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include <stdio.h>

struct list frame_table;
struct lock frame_lock;

static bool
install_page (void *upage, void *kpage, bool writable);

void frame_init()
{
    lock_init(&frame_lock);
    list_init(&frame_table);
}

struct page* spt_find(struct hash* h, void *addr)
{
    if(!is_user_vaddr(addr))
        return NULL;
    struct page p;
    p.vaddr=pg_round_down(addr);
    struct hash_elem* e=hash_find(h, &p.elem);
    if(e==NULL)
        {
            return NULL;
        }
    return hash_entry(e, struct page, elem);
}

struct page* spt_add(struct hash* h, struct page* p)
{
    hash_insert(h, &p->elem);
    return NULL;
}

struct hash_elem* spt_del(struct hash* h, struct page *p)
{
    struct hash_elem *e=hash_delete(h, &p->elem);
    return e;
}

void* get_frame(void* paddr, enum palloc_flags flag)
{
    lock_acquire(&frame_lock);
    struct frame *f =malloc(sizeof(struct frame));
    void* addr=palloc_get_page(PAL_USER | flag);
    if(addr==NULL)
        //swap_out();
        {
            lock_release(&frame_lock);
            return NULL;
        }
    else{
        f->addr=addr;
        f->page_addr=paddr;
    }
    list_push_back(&frame_table, &f->frame_elem);
    lock_release(&frame_lock);
    return addr;
}

void frame_free(struct frame* f)
{
    lock_acquire(&frame_lock);
    list_remove(&f->frame_elem);
    palloc_free_page(f->addr);
    free(f);
    lock_release(&frame_lock);
}

struct frame* find_frame(void* addr)
{
    struct list_elem *e=list_front(&frame_table);
    struct frame *f = list_entry(e, struct frame, frame_elem);

    for(;;)
    {
        if(f->addr==addr)
            return f;
        if(e!=list_end(&frame_table))
            e=list_next(e);
        else
            break;
    }
    return NULL;
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
    printf("%p\n", addr);
    if(p==NULL)
        {
            return false;
        }
    if(p->frame_by_page!=NULL)
        return true;

    addr=pg_round_down(addr);
    void *faddr=get_frame(addr, PAL_USER);
    p->frame_by_page=find_frame(faddr);
    struct spt_data* d=p->data;

    file_seek (d->file, d->ofs);
    if (file_read (d->file, p->frame_by_page->addr, d->read_bytes) != (int) d->read_bytes)
        {
          frame_free(p->frame_by_page);
          p->frame_by_page=NULL;
          return false;
        }
      memset (p->frame_by_page->addr + d->read_bytes, 0, PGSIZE - d->read_bytes);

     if (!install_page (p->vaddr, faddr, p->data->writable_flag)) 
        {
          frame_free(p->frame_by_page);
          return false; 
        }
    return true;
}
/*
bool expand_stack(void* addr, struct intr_frame *f)
{
    void *rsp_stack = is_kernel_vaddr(f->esp) ? thread_current()->esp_stack : f->esp;
    if(rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK)
    {
        palloc_get_page(thread_current()->esp_stack - PGSIZE);
        thread_current()->esp_stack -= PGSIZE;
        return true;
    }
    return false;
}*/

static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
