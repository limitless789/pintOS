#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  switch(*(uint32_t*)(f->esp))
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_address(f->esp + 4);
      exit((int)*(uint32_t*)(f->esp + 4));
      break;
    case SYS_EXEC:
      check_address(f->esp + 4);
      f->eax = exec((const char*)*(uint32_t*)(f->esp + 4));
      break;
    case SYS_WAIT:
      check_address(f->esp + 4);
      f->eax = wait((pid_t)*(uint32_t*)(f->esp + 4));
      break;
    case SYS_CREATE:
      check_address(f->esp + 4);
      check_address(f->esp + 8);
      f->eax = create((const char*)*(uint32_t*)(f->esp + 4), (unsigned)*(uint32_t*)(f->esp + 8));
      break;
    case SYS_REMOVE:
      check_address(f->esp + 4);
      f->eax = remove((const char*)*(uint32_t*)(f->esp + 4));
      break;
    case SYS_OPEN:
      check_address(f->esp + 4);
      f->eax = open((const char*)*(uint32_t*)(f->esp + 4));
      break;
    case SYS_FILESIZE:
      check_address(f->esp + 4);
      f->eax = filesize((int)*(uint32_t*)(f->esp + 4));
      break;
    case SYS_READ:
      check_address(f->esp + 4);
      check_address(f->esp + 8);
      check_address(f->esp + 12);
      f->eax = read((int)*(uint32_t*)(f->esp + 4), (void*)*(uint32_t*)(f->esp + 8), (unsigned)*(uint32_t*)(f->esp + 12));
      break;
    case SYS_WRITE:
      check_address(f->esp + 4);
      check_address(f->esp + 8);
      check_address(f->esp + 12);
      f->eax = write((int)*(uint32_t*)(f->esp + 4), (void*)*(uint32_t*)(f->esp + 8), (unsigned)*(uint32_t*)(f->esp + 12));
      break;
    case SYS_SEEK:
      check_address(f->esp + 4);
      check_address(f->esp + 8);
      seek((int)*(uint32_t*)(f->esp + 4), (unsigned)*(uint32_t*)(f->esp + 8));
      break;
    case SYS_TELL:
      check_address(f->esp + 4);
      f->eax = tell((int)*(uint32_t*)(f->esp + 4));
      break;
    case SYS_CLOSE:
      check_address(f->esp + 4);
      close((int)*(uint32_t*)(f->esp + 4));
      break;
  }
}

void check_address(void* address)
{
  if(is_kernel_vaddr(address))
    exit(-1);
}

void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
  struct thread* cur = thread_current();
  int i;
  printf("%s: exit(%d)\n", cur->name, status);
  cur->child_exit_status = status;
  for(i = 3; i < 128; i++)
    if(cur->file_descriptor[i] != NULL)
      close(i);
  struct thread* tmp_thread;
  struct list_elem* tmp_elem;
  for(tmp_elem = list_begin(&thread_current()->child_thread); tmp_elem != list_end(&thread_current()->child_thread); tmp_elem = list_next(tmp_elem))
  {
    tmp_thread = list_entry(tmp_elem, struct thread, child_thread_elem);
    process_wait(tmp_thread->tid);
  }
  thread_exit();
}

pid_t exec(const char* cmd_lines)
{
  struct file *file = NULL;
  int i;
  char filename[128];
  char *fn_copy=palloc_get_page(0);
  for(i = 0; cmd_lines[i] != ' ' && cmd_lines[i] != '\0'; i++)
    filename[i] = cmd_lines[i];
  filename[i] = '\0';
  file = filesys_open(filename);
  if(fn_copy==NULL)
    {
      palloc_free_page (fn_copy);
      return -1;
    }
  strlcpy (fn_copy, cmd_lines, PGSIZE);
  if(file == NULL)
    return -1;
  //file_deny_write(file);
  tid_t result = process_execute(fn_copy);
  return (pid_t) result;
}

int wait(pid_t pid)
{
  return process_wait((tid_t)pid);
}

bool create(const char *file, unsigned initial_size)
{
  if(file == NULL)
    exit(-1);
  check_address(file);
  return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
  if(file == NULL)
    exit(-1);
  check_address(file);
  return filesys_remove(file);
}

int open(const char* file)
{
  if(file == NULL)
    exit(-1);
  check_address(file);
  lock_acquire(&file_lock);
  int i, tmp;
  struct file* open_file = filesys_open(file);
  tmp = -1;
  if(open_file == NULL)
    tmp = -1;
  else
  {
    for(i = 3; i < 128; i++)
    {
      if(thread_current()->file_descriptor[i] == NULL)
      {
        if(strcmp(thread_name(), file) == 0)
        {
          file_deny_write(open_file);
        }
        thread_current()->file_descriptor[i] = open_file;
        tmp = i;
        break;
      }
    }
  }
  lock_release(&file_lock);
  return tmp;
}

int filesize(int fd)
{
  if(thread_current()->file_descriptor[fd] == NULL)
    exit(-1);
  return (int)file_length(thread_current()->file_descriptor[fd]);
}

int read(int fd, void* buffer, unsigned size)
{
  int i;
  check_address(buffer);
  lock_acquire(&file_lock);
  if(fd == 0)
  {
    for(i=0; i < size; i++)
   {
      if(input_getc() == '\0')
        break;
    }
  }
  else if(fd > 2)
  {
    struct thread* cur = thread_current();
    if(cur->file_descriptor[fd] == NULL)
    {
      //lock_release(&file_lock);
      exit(-1);
    }
    i = file_read(cur->file_descriptor[fd], buffer, size);
  }
  lock_release(&file_lock);
  return i;
}

int write(int fd, void* buffer, unsigned size)
{
  check_address(buffer);
  lock_acquire(&file_lock);
  int tmp = 0;
  if(fd == 1)
  {
    putbuf(buffer, size);
    tmp = size;
  }
  else if(fd > 2)
  {
    if(thread_current()->file_descriptor[fd] == NULL)
    {
      lock_release(&file_lock);
      exit(-1);
    }
    struct file* cur_file = thread_current()->file_descriptor[fd];
    if(cur_file->deny_write)
      {
        file_deny_write(cur_file);
      }
    tmp = file_write(cur_file, buffer, size);  
  }
  lock_release(&file_lock);
  return tmp;
}

void seek(int fd, unsigned position)
{
  if(thread_current()->file_descriptor[fd] == NULL)
    exit(-1);
  file_seek(thread_current()->file_descriptor[fd], position);
}

unsigned tell(int fd)
{
  if(thread_current()->file_descriptor[fd] == NULL)
    exit(-1);
  return (unsigned)file_tell(thread_current()->file_descriptor[fd]);
}

void close(int fd)
{
  if(thread_current()->file_descriptor[fd] == NULL)
    exit(-1);
  struct file* cur_file = thread_current()->file_descriptor[fd];
  thread_current()->file_descriptor[fd] = NULL;
  return file_close(cur_file);
}
