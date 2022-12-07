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

struct frame* get_frame(struct page* p, enum palloc_flags pf)
{
    lock_acquire(&frame_lock);
    struct frame *f =malloc(sizeof(struct frame));
    void* addr=palloc_get_page(PAL_USER | pf);
    if(addr==NULL)
        //swap_out();
        return NULL;
    else{
        f->addr=addr;
        f->page_of_frame=p;
    }
    list_push_back(&frame_table, &f->frame_elem);
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
        {
            return false;
        }
    if(p->frame_by_page!=NULL)
        return true;

    addr=pg_round_down(addr);
    struct frame* f=get_frame(p, NULL);
    p->frame_by_page=f;
    struct spt_data* d=p->data;

    file_seek (d->file, d->ofs);
    if (file_read (d->file, p->frame_by_page->addr, d->read_bytes) != (int) d->read_bytes)
        {
          frame_free(f);
          p->frame_by_page=NULL;
          return false;
        }
      memset (p->frame_by_page->addr + d->read_bytes, 0, PGSIZE - d->read_bytes);

     if(pagedir_get_page (t->pagedir, addr) != NULL || !pagedir_set_page (t->pagedir, addr, f->addr, p->data->writable_flag))
        {
            frame_free(f);
            p->frame_by_page=NULL;
            return false;
        }
    return true;
}

void* do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
    struct file *re_file = file_reopen(file);
    void *tmp = addr;
    size_t read_bytes = length > file_length(file) ? file_length(file) : length;
    size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

    while(read_bytes > 0 || zero_bytes > 0){
        size_t page_read_bytes;
        if(read_bytes < PGSIZE)
            page_read_bytes = read_bytes;
        else
            page_read_bytes = PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        struct container *container = (struct container*)malloc(sizeof(struct container));
        container->file = re_file;
        container->offset = offset;
        container->page_read_bytes = page_read_bytes;

        if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load, container)){
            return NULL;
        }
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        addr += PGSIZE;
        offset += page_read_bytes;
    }
    return tmp;
}

void* do_munmap(void *addr)
{
    while (1)
    {
      struct page* page = spt_find(&thread_current()->spt, addr);
        if (page == NULL)
            break;
        struct container * aux = (struct container *) page->uninit.aux;
        
        if(pagedir_is_dirty(thread_current()->pagedir, page->vaddr)) {
            file_write_at(aux->file, addr, aux->page_read_bytes, aux->offset);
            pagedir_set_dirty (thread_current()->pagedir, page->vaddr, 0);
        }

        pagedir_clear_page(thread_current()->pagedir, page->vaddr);
        addr += PGSIZE;
    }
}

bool pagedir_is_dirty (uint32_t *pagedir, const void *vpage)
{
	uint32_t *pte = pml4e_walk (pagedir, (uint32_t) vpage, false);
	return pte != NULL && (*pte & PTE_D) != 0;
}

void pagedir_set_dirty (uint32_t *pagedir, const void *vpage, bool dirty)
{
	uint32_t *pte = pml4e_walk (pagedir, (uint32_t) vpage, false);
	if (pte) {
		if (dirty)
			*pte |= PTE_D;
		else
			*pte &= ~(uint32_t) PTE_D;
		if (rcr3 () == vtop (pagedir))
			invlpg ((uint64_t) vpage);
	}
}

void pagedir_clear_page (uint32_t *pagedir, void *upage)
{
	uint32_t *pte;
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (is_user_vaddr (upage));

	pte = pml4e_walk (pagedir, (uint32_t) upage, false);

	if (pte != NULL && (*pte & PTE_P) != 0)
    {
		*pte &= ~PTE_P;
		if (rcr3 () == vtop (pagedir))
			invlpg ((uint32_t) upage);
	}
}

bool vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;

	if (spt_find(spt, upage) == NULL)
    {
		struct page* page = (struct page*)malloc(sizeof(struct page));

        typedef bool (*initializerFunc)(struct page *, enum vm_type, void *);
        initializerFunc initializer = NULL;

        switch(VM_TYPE(type)){
            case VM_ANON:
                initializer = anon_initializer;
                break;
            case VM_FILE:
                initializer = file_backed_initializer;
                break;

        uninit_new(page, upage, init, type, aux, initializer);

        page->writable = writable;
		return spt_add(spt, page);
	}
err:
	return false;
}

void supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED)
{
    struct hash_iterator i;

    hash_first(&i, &spt->pages);
    while(hash_next(&i)){
        struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);

        if(page->operations->type == VM_FILE){
            do_munmap(page->vaddr);
        }
    }
    hash_destroy(&spt->pages, spt_destructor);
}

bool vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
    struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
    if(is_kernel_vaddr(addr))
        return false;
    void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
    if(not_present)
    {
        if(!vm_claim_page(addr))
        {
            if(rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK)
            {
                vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
                return true;
            }
            return false;
        }
        else
            return true;
    }
    return false;
}

static void vm_stack_growth (void *addr UNUSED)
{
    if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
    {
        vm_claim_page(addr);
        thread_current()->stack_bottom -= PGSIZE;
    }
}

void vm_anon_init (void) 
{
    swap_disk = disk_get(1,1);
    size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
    swap_table = bitmap_create(swap_size);
}

static bool anon_swap_out (struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    int page_no = bitmap_scan(swap_table, 0, 1, false);
    if(page_no == BITMAP_ERROR)
        return false;
    for(int i=0; i<SECTORS_PER_PAGE; ++i)
        disk_write(swap_disk, page_no *SECTORS_PER_PAGE + i, page->va + DISK_SECTOR_SIZE * i);

    bitmap_set(swap_table, page_no, true);
    pagedir_clear_page(thread_current()->pagedir, page->vaddr);
    anon_page->swap_index = page_no;
    return true;
}

static bool anon_swap_in (struct page *page, void *kva)
{
    struct anon_page *anon_page = &page->anon;
    int page_no = anon_page->swap_index;

    if(bitmap_test(swap_table, page_no) == false)
        return false;

    for(int i=0; i< SECTORS_PER_PAGE; ++i)
        disk_read(swap_disk, page_no * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
    bitmap_set(swap_table, page_no, false);
    return true;
}