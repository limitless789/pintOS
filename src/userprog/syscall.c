#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  get_argument();
  switch(*(uint32_t*)(f->esp))
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_address(f->esp + WORD);
      exit((int)*(uint32_t*)(f->esp + WORD));
      break;
    case SYS_EXEC:
      check_address(f->esp + WORD);
      f->eax = exec((const char*)*(uint32_t*)(f->esp + WORD));
      break;
    case SYS_WAIT:
      check_address(f->esp + WORD);
      f->eax = wait((pid_t)*(uint32_t*)(f->esp + WORD));
      break;
    case SYS_CREATE:
      check_address(f->esp + WORD);
      check_address(f->esp + WORD + WORD);
      f->eax = create((const char*)args[1], (unsigned)args[2]);
      break;
    case SYS_REMOVE:
      check_address(f->esp + WORD);
      f->eax = remove((const char*)args[1]);
      break;
    case SYS_OPEN:
      check_address(f->esp + WORD);
      f->eax = open((const char*)args[1]);
      break;
    case SYS_FILESIZE:
      check_address(f->esp + WORD);
      f->eax = filesize((const char*)args[1]);
      break;
    case SYS_READ:
      check_address(f->esp + WORD);
      f->eax = read((const char*)args[1], (void*)args[2], (unsigned)args[3]);
      break;
    case SYS_WRITE:
      check_address(f->esp + WORD);
      f->eax = write((const char*)args[1], (void*)args[2], (unsigned)args[3]);
      break;
    case SYS_SEEK:
      check_address(f->esp + WORD);
      seek((const char*)args[1], (unsigned)args[2]);
      break;
    case SYS_TELL:
      check_address(f->esp + WORD);
      f->eax = tell((const char*)args[1]);
      break;
    case SYS_CLOSE:
      check_address(f->esp + WORD);
      close((const char*)args[1]);
      break;
  }
}

void check_address(void* address)
{
  if(is_kernel_vaddr(address))
    exit(-1);
}

void get_argument(void *esp, void *arg, int count)
{

}

void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
  struct thread* cur = thread_current();
  char filename[128];
  int idx;
  for(idx = 0; (cur->name)[idx] != ' ' && (cur->name)[idx] != '\0'; idx++)
    filename[idx] = (cur->name)[idx];
  filename[idx] = '0';
  printf("%s: exit(%d)\n", filename, status);
  cur->child_exit_status = status;
  struct thread* current_thread = thread_current();
  for(int i = 3; i < 128; i++)
    if(current_thread->file_descriptor[i] != NULL)
      close(i);
  struct thread* tmp_thread;
  struct list_elem* tmp_elem;
  for(tmp_elem = list_begin(&thread_currnet()->child_thread); tmp_elem != list_end(&thread_current()->child_thread); tmp_elem = list_next(tmp_elem))
  {
    tmp_thread = list_entry(tmp_elem, struct thread, child_thread_elem);
    process_wait(tmp_thread->tid);
  }
  thread_exit();
}

pit_t exec(const char* cmd_lines)
{
  struct file *file = NULL;
  int idx;
  char filename[128];
  for(idx = 0; cmd_lines[idx] != ' ' && cmd_lines[idx] != '\0'; idx++)
    filename[idx] = cmd_lines[idx];
  filename[idx] = '\0';
  file = filesys_open(filename);
  if(file == NULL)
    return -1;
  else
  {
    tid_t result = process_execute(cmd_lines);
    return (pid_t) result;
  }
}

tid_t process_execute(const char *filename)
{
  char *fn_copy;
  tid_t tid;
  fn_copy = palloc_get_page(0);
  if(fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, filename, PGSIZE);
  tid = thread_create(filename, PRI_DEFAULT, start_process, fn_copy);
  if(tid == TID_ERROR)
    palloc_free_page(fn_copy);
  return tid;
} 

int wait(pid_t pid)
{
  return process_wait((tid_t)pid);
}

int process_wait(tid_t child_tid)
{
  int exit_status;
  struct thread* cur = thread_current();
  struct thread* cur_thread;
  struct list_elem* tmp;
  for(tmp = list_begin(&cur->child_thread); tmp == list_end(&cur->child_thread); tmp = list_next(tmp))
  {
    cur_thread = list_empty(tmp, struct thread, child_thread_elem);
    if(child_tid == cur_thread->tid)
    {
      sema_down(&(cur_thread->memory_preserve));
      exit_status = cur_thread->child_exit_status;
      list_remove(&(cur_thread->child_thread_elem));
      sema_up(&(cur_thread->child_thread_lock));
      return exit_status;
    }
  }
  return -1;
}

void process_exit(void)
{
  struct thread* cur = thread_current();
  uint32_t* pd;
  pd = cur->pagedir;
  if(pd != NULL)
  {
    cur->pagedir = NULL;
    pagedir_activate(NULL);
    pagedir_destroy(pd);
  }
  sema_up(&(cur->memory_preserve));
  sema_down(&(cur->child_thread_lock));
}

bool create(const char *file, unsigned initial_size)
{

}

bool remove(const char *file)
{

}

int open(const char* file)
{

}

int filesize(int fd)
{

}

int read(int fd, void* buffer, unsigned size)
{
  int i;
  if(fd != stdin)
    return -1;
  for(i=0; i < size; i++)
  {
    if(input_getc() == '\0')
      break;
  }
  return i;
}

int write(int fd, void* buffer, unsigned size)
{
  if(fd != stdout)
    return -1;
  putbuf(buffer, size);
  return size;
}

void seek(int fd, unsigned position)
{

}

unsigned tell(int fd)
{

}

void close(int fd)
{

}