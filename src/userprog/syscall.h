#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void check_address(void* address);
void get_argument(void *esp, void *arg, int count);
void halt(void);
void exit(int status);
pit_t exec(const char* cmd_lines);
tid_t process_execute(const char *filename);
int wait(pid_t pid);
int process_wait(tid_t child_tid);
void process_exit(void);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void* buffer, unsigned size);
int write(int fd, void* buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

#endif /* userprog/syscall.h */
