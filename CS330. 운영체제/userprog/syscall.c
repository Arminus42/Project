#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "string.h"

/* P2. 파일 관련 함수가 있는 헤더파일 추가 */
#include "filesys/file.h"
#include "filesys/filesys.h"

/* P2. Lock을 위한 헤더파일 추가 */
#include "threads/synch.h"

/* P2. 키보드 입력 함수가 있는 헤더파일 추가 */
#include "devices/input.h"

#include "userprog/process.h"

/* P2. 파일 계열 함수를 여러 프로세스가 동시에 호출하지 못하게 동기화하는 Lock */
// struct lock file_lock;

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

#define is_invalid_fd(fd) (fd < 2 || fd >= FD_MAX || cur->fds == NULL || cur->fds[fd] == NULL)

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* Lock 초기화 */
	// lock_init(&file_lock);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
/* [P2] System Handler 구현 */
void
syscall_handler (struct intr_frame *f) {
	uint64_t syscall_num = f->R.rax; // System call number
	// msg("syscall: %ld", syscall_num);
    thread_current()->user_rsp = f->rsp; // [P3-3] user stack 포인터 저장
	switch (syscall_num){
        case SYS_HALT:
            halt();
            break;
		case SYS_EXIT:
			exit((int)f->R.rdi);
			break;
		case SYS_FORK:
			thread_current()->parent_if = *f;
			f->R.rax = fork((const char *) f->R.rdi);
			break;
		case SYS_EXEC:
			f->R.rax = exec((const char *) f->R.rdi);
			break;
		case SYS_WAIT:
            f->R.rax = wait((pid_t) f->R.rdi);
            break;
		case SYS_CREATE:
			f->R.rax = create((const char *) f->R.rdi, (unsigned) f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove((const char *)f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open((const char *)f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize((int)f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
			break;
		case SYS_WRITE:
            f->R.rax = write((int)f->R.rdi, (const void *)f->R.rsi, (unsigned)f->R.rdx);
            break;
        case SYS_SEEK:
            seek((int)f->R.rdi, (unsigned)f->R.rsi);
            break;
        case SYS_TELL:
            f->R.rax = tell((int)f->R.rdi);
            break;
        case SYS_CLOSE:
            close((int)f->R.rdi);
            break;
        case SYS_MMAP:
            // TODO: P3. mmap syscall
            f->R.rax = mmap((void *)f->R.rdi, (size_t)f->R.rsi, (int)f->R.rdx, (int)f->R.r10, (off_t)f->R.r8);
            break;
        case SYS_MUNMAP:
            // TODO: P3. munmap syscall
            munmap((void *)f->R.rdi);
            break;
        /* [P4-2] syscall */
        case SYS_CHDIR:
            f->R.rax = chdir((char*)f->R.rdi);
            break;
        case SYS_MKDIR:
            f->R.rax = mkdir((char*)f->R.rdi);
            break;
        case SYS_READDIR:
            f->R.rax = readdir((int)f->R.rdi, (char *)f->R.rsi);
            break;
        case SYS_ISDIR:
            f->R.rax = isdir((int)f->R.rdi);
            break;
        case SYS_INUMBER:
            f->R.rax = inumber((int)f->R.rdi);
            break;
        case SYS_SYMLINK:
            f->R.rax = symlink((char*)f->R.rdi, (char *)f->R.rsi);
            break;
            
        default:
            printf("Unknown system call: %lu\n", syscall_num);
            thread_exit();
    }
	/*
	printf ("system call!\n");
	printf ("intr vector no.%ld\n", f->vec_no);
	ASSERT(0);
	thread_exit ();
	*/
}

/* P2. Pintos를 종료시키는 system call */
void halt(void){
    power_off();
}

/* P2. 현재 유저 프로그램을 종료하고 status 커널로 돌아가는 system call */
void exit(int status){
    struct thread *cur = thread_current();
    cur->exit_status = status; // P2. 종료 상태 정보 저장
    //printf("%s: exit(%d)\n", cur->name, status); // P2. 종료 메시지 출력
    thread_exit();
}

/* P2. 현재 프로세스의 복제본을 만드는 system call */
pid_t fork(const char *thread_name){
    // P2. 함수 인자 유효성 검사
    is_valid_ptr(thread_name);

    pid_t pid = process_fork(thread_name);
    return pid;
}

/* P2. 현재 프로세스를 cmd_line으로 지정된 프로그램으로 변경하는 system call */
int exec(const char *cmd_line){
    // P2. 함수 인자 유효성 검사
    is_valid_ptr(cmd_line);

    // P2. 유저 공간에 있는 cmd_line을 커널 공간으로 복사 => process_exec로 이동

    // P2. 복사된 cmd_line을 process_exec에 넘김
    if(process_exec((void *) cmd_line) == -1) exit(-1);
    
	NOT_REACHED();
}

/* P2. 자식 프로세스의 종료를 기다리는 system call */
int wait(pid_t pid){
    return process_wait(pid);
}

/* P2. 새 파일을 생성하고 성공 여부를 반환하는 system call */
bool create(const char *file, unsigned initial_size){
    // P2. 함수 인자 유효성 검사
    is_valid_ptr(file);
	
    // P2. 파일 생성 & syscall 동기화
	process_lock_file();
    bool result = filesys_create(file, initial_size);
    process_release_file(&file_lock);
	
    return result;
}

/* P2. 파일을 삭제하고 성공 여부를 반환하는 system call */
bool remove(const char *file){
    // P2. 함수 인자 유효성 검사
    is_valid_ptr(file);

    // P2. 파일 삭제 & syscall 동기화
	process_lock_file(&file_lock);
    bool result = filesys_remove(file);
    process_release_file(&file_lock);

    return result;
}

/* P2. 파일을 열고 FD를 반환하는 system call */
int open(const char *file){
    // P2. 함수 인자 유효성 검사
    is_valid_ptr(file);
    struct thread *cur = thread_current();


	// P2. 파일 열기 & syscall 동기화
	process_lock_file(&file_lock);
    struct file *opened_file = filesys_open(file);
	process_release_file(&file_lock);
    if(opened_file == NULL) return -1;

    // P2. FD 테이블의 빈 자리에 저장 (표준 입출력 0,1 제외) -> EXTRA: 0,1도 포함시키기
    for(int fd = 2; fd < FD_MAX; fd++){
        if(cur->fds[fd] == NULL){
            cur->fds[fd] = opened_file;
            return fd;
        }
    }

    // P2. 파일 닫기 & syscall 동기화
	process_lock_file(&file_lock);
    file_close(opened_file);
    process_release_file(&file_lock);
    return -1;
}

/* P2. 파일 사이즈를 반환하는 system call */
int filesize(int fd){
    struct thread *cur = thread_current();

    // P2. FD가 유효한지 검사 -> EXTRA: fd<0
    if(fd < 2 || fd >= FD_MAX) return -1;

	// P2. 해당 FD가 열린 파일인지 검사
    struct file *f = cur->fds[fd];
    if(f == NULL) exit(-1);

    // P2. 파일 크기 반환 & syscall 동기화
	process_lock_file(&file_lock);
    int result = file_length(f);
    process_release_file(&file_lock);
    return result; 
}

/* P2. 파일을 읽는 system call */
int read(int fd, void *buffer, unsigned size){
    // P2. 함수 인자 유효성 검사
    is_valid_ptr(buffer);

    for(unsigned i = 0; i < size; i++) is_valid_ptr((uint8_t *)buffer + i); // [P3-2] buffer가 걸친 모든 주소가 접근 가능한지 확인 (Lazy page 발생 가능성)

    // [P3-2] buffer가 걸친 모든 주소가 쓰기 가능한 주소인지 확인
    for(unsigned i = 0; i < size; i++) is_valid_ptr_writable((uint8_t *)buffer + i);

    struct thread *cur = thread_current();

	// P2. FD가 stdin인 경우
    if(fd == 0){
        for(unsigned i = 0; i < size; i++) ((uint8_t *)buffer)[i] = input_getc();
        return size;
    }
	
	// P2. FD가 유효하지 않은 경우 -> EXTRA: fd<0
	if(is_invalid_fd(fd)) return -1;

    // P2. 파일 읽기 & syscall 동기화
	process_lock_file(&file_lock);
    int result = file_read(cur->fds[fd], buffer, size);
    process_release_file(&file_lock);
    return result;
}

/* P2. 파일을 쓰는 system call */
int write(int fd, const void *buffer, unsigned size){
    // P2. 함수 인자 유효성 검사
    is_valid_ptr(buffer);

    struct thread *cur = thread_current();

    // P2. FD가 stdout인 경우
    if(fd == 1){
        putbuf((char *) buffer, size);
        return size;
    }

    // P2. FD가 유효하지 않은 경우 -> EXTRA: fd<0
    if(is_invalid_fd(fd)) return -1;
	
    // P2. 파일 쓰기 & syscall 동기화
	process_lock_file(&file_lock);
    /* [P4-2] fd가 dir를 가리키면 write 불가 */
    int result = 0;
    if(filesys_is_dir(cur->fds[fd]))
        result = -1;
    else
        result = file_write(cur->fds[fd], buffer, size);
    process_release_file(&file_lock);
    return result; 
}

/* P2. 열린 FD의 파일 포인터 위치를 이동하는 system call */
void seek(int fd, unsigned position){
    struct thread *cur = thread_current();

    // P2. FD가 유효하지 않은 경우 -> EXTRA: fd<0
    if(is_invalid_fd(fd)) return;

	// P2. 파일 포인터 위치 이동 & syscall 동기화
	process_lock_file();
    file_seek(cur->fds[fd], position);
    process_release_file();
}

/* P2. 열린 FD의 파일 포인터 위치를 반환하는 system call */
unsigned tell(int fd){
    struct thread *cur = thread_current();

    // P2. FD가 유효하지 않은 경우 -> EXTRA: fd<0
    if(is_invalid_fd(fd)) return -1;

    // P2. 파일 포인터 위치 반환 & syscall 동기화
	process_lock_file();
	unsigned result = file_tell(cur->fds[fd]);
	process_release_file();
	return result;
}

/* P2. 파일을 닫는 system call */
void close(int fd){
    struct thread *cur = thread_current();

    // P2. FD가 유효하지 않은 경우 -> EXTRA: fd<0
    if(is_invalid_fd(fd)) return;

    struct file *f = cur->fds[fd];
    if(f == NULL) return;

    // P2. 파일 닫기 & syscall 동기화
    process_lock_file();
    file_close(f);
    process_release_file();

    // P2. FD 테이블에서 현재 FD 제거
    cur->fds[fd] = NULL;
}

/* P2. 함수 인자가 유효한지 검사하는 함수 */ 
void is_valid_ptr(const void *ptr){
    if(ptr == NULL || !is_user_vaddr(ptr)) exit(-1); // [P2] NULL이거나 커널 주소인 경우 종료
    if(pml4_get_page(thread_current()->pml4, ptr) == NULL){
        if (!vm_claim_page((void *)ptr)) exit(-1);  // [P3-2] lazy allocation된 페이지 가능성 체크 (실제 물리 프레임 할당 시도 후 실패시 종료)
    }
}
/* P3. mmap system call */
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset) {
    struct thread *cur = thread_current();

    // P3.addr 유효성 체크
    if (addr == NULL || pg_ofs(addr) != 0 || length == 0)
        return NULL;

    // P3. FD가 유효하지 않은 경우(stdio 제외)
    if (fd < 2 || fd >= FD_MAX || cur->fds[fd] == NULL)
        return NULL;

    struct file *file = cur->fds[fd];
    if (file == NULL)
        return NULL;

    // P3. 파일 매핑 & syscall 동기화
    process_lock_file(&file_lock);
    void *mapped_addr = do_mmap(addr, length, writable, file, offset);
    process_release_file(&file_lock);

    return mapped_addr;
}

/* P3. munmap system call */
void munmap(void *addr) {
    // addr 유효성 체크
    if (addr == NULL || pg_ofs(addr) != 0)
        return;

    process_lock_file(&file_lock);
    do_munmap(addr);
    process_release_file(&file_lock);
}

bool is_valid_dir_name(const char* dir);

bool chdir(const char *dir){
    if(!is_valid_dir_name(dir))
        return false;

    bool success;

    process_lock_file(&file_lock);
    success = filesys_change_dir(dir);
    process_release_file(&file_lock);

    return success;
}

bool mkdir(const char *dir){
    if(!is_valid_dir_name(dir))
        return false;
    
    bool success;

    process_lock_file(&file_lock);
    success = filesys_create_dir(dir);
    process_release_file(&file_lock);

    return success;
}

bool readdir(int fd, char *name){
    struct thread* cur = thread_current();

    if(is_invalid_fd(fd)) 
        return false;

    bool success;
    process_lock_file(&file_lock);

    success = filesys_read_dir(cur->fds[fd], name);

    process_release_file(&file_lock);
    return success;
}

bool isdir(int fd){
    bool success;
    process_lock_file(&file_lock);

    success = filesys_is_dir(thread_current()->fds[fd]);

    process_release_file(&file_lock);
    return success;
}

int inumber(int fd){
    int result;
    process_lock_file(&file_lock);

    result = filesys_inumber(thread_current()->fds[fd]);

    process_release_file(&file_lock);
    return result;
}

int symlink(const char *target, const char *linkpath){
    int result;
    if(target == NULL || strlen(target) == 0)
        return -1;
    if(linkpath == NULL || strlen(linkpath) == 0)
        return -1;

    process_lock_file(&file_lock);

    result = filesys_symlink(target, linkpath);

    process_release_file(&file_lock);
    return result;
}

/* [P3-2] 쓰기 가능한 유저 주소인지 검사하는 함수 */
void is_valid_ptr_writable(const void *ptr){
    if(ptr == NULL || !is_user_vaddr(ptr)) exit(-1); // [P3-2] NULL이거나 커널 주소 시 종료
    
    struct page *p = spt_find_page(&thread_current()->spt, ptr);
    if(p == NULL){
        if (!vm_claim_page((void *)ptr)) exit(-1); // [P3-2] Lazy allocation 실패 시 종료
        p = spt_find_page(&thread_current()->spt, ptr);
    }

    if(p == NULL || !p->writable) exit(-1); // [P3-2] 페이지가 없거나 쓰기 권한이 없으면 종료
}

/* [P4-2] 유효한 dir 이름인지 확인 */
bool is_valid_dir_name(const char* name){
    if(name == NULL || strlen(name) == 0) 
        return false;
    return true;
}