#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"
#include "threads/vaddr.h"

struct page;
enum vm_type;

struct file_page {
    struct file *file;        /* 매핑된 파일 */
    off_t offset;            /* 파일 내 오프셋 */
    size_t read_bytes;       /* 읽을 바이트 수 */
    size_t zero_bytes;       /* 0으로 채울 바이트 수 */
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
