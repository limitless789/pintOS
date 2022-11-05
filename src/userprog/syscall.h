#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock file_lock;

void syscall_init (void);
void check_address(void* address);
void halt(void);
void exit(int status);
pit_t exec(const char* cmd_lines);
int wait(pid_t pid);
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
