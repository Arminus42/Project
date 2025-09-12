#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

#endif /* userprog/syscall.h */

/* P2. bool 헤더파일 추가 */
#include <stdbool.h>
/* P3. size_t, off_t 헤더파일 추가 */
#include <stddef.h>
#include "filesys/off_t.h"

/* P2. 커널에서 pid_t 타입 선언 */
typedef int pid_t;

/* P2. system call 함수 선언 */
void halt(void);
void exit(int status);
pid_t fork(const char *thread_name);
int exec(const char *cmd_line);
int wait(pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
void is_valid_ptr(const void *ptr);
/* P3. mmap, munmap syscall */
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);
bool chdir(const char *dir);
bool mkdir(const char *dir);
bool readdir(int fd, char *name);
bool isdir(int fd);
int inumber(int fd);
int symlink(const char *target, const char *linkpath);

void check_buffer(const void *buffer, unsigned size);
void is_valid_ptr_writable(const void *ptr);










