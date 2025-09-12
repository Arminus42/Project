#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h" // P2. semaphore 구조체 사용
#ifndef VM
#define VM
#endif

#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* P2. FD 크기 제한 */
#define FD_MAX 130

#define USERPROG

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */

	/* [P1-1] 깨어날 시간 값 */
	int64_t wakeup_tick;

	/* [P1-2] 다른 스레드 점유가 해제되기를 기다리고 있는 lock */
	struct lock *waiting_lock;

	/* [P1-2] 우선순위를 기부해준 스레드들의 연결 리스트 */
	struct list donations;

	/* [P1-2] 다른 스레드에게 우선순위를 기부할 때 donations에 넣을 elem */
	struct list_elem donation_elem;

	/* [P1-2] 스레드 생성 시 받았던 최초의 우선순위 */
	int init_priority; 

	/* [P1-3] MLFQS 구현에 필요한 값 */
	int nice;
	int recent_cpu;
	struct list_elem allelem;   

	/* P2. 자식이 부모에게 전달할 종료 상태 */
	int exit_status;

	/* P2. 부모가 자식에게 전달할 레지스터 상태 */
	struct intr_frame parent_if;
	
	/* P2. 기다린 전적 플래그 */
	bool has_been_waited;
	
	/* P2. 부모가 기다리면 깨우는 세마포어 */
	struct semaphore wait_sema;

	/* P2. 자식이 종료되면 깨우는 세마포어 */
	struct semaphore exit_sema;

	/* P2. 자식 목록과 자식 리스트에 넣어지기 위한 elem */
	struct list child_list;
	struct list_elem child_elem;

	/* P2. FD 집합 */
	struct file *fds[FD_MAX];

	/* kerenel level process인지 확인용 */
	bool is_kernel;
	struct file *running_file;

	/* fork시 dup 성공인지 아닌지 판별 용 */
	bool is_dup_success;

	struct lock fork_lock;

	struct semaphore process_init_sema;

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
	uint64_t user_rsp;  // [P3-3] 사용자 rsp 저장
#endif
#ifdef EFILESYS
	struct dir* pwd;					/* [P4-2] 현재 프로세스의 pwd */
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

/* [P1-1] 헤더 파일 선언 */
void thread_sleep(int64_t ticks);
bool thread_wakeup(int64_t ticks);
bool compare_wakeup_tick(const struct list_elem *a, const struct list_elem *b, void *aux);

/* [P1-2] 헤더 파일 선언 */
bool compare_priority(const struct list_elem *a, const struct list_elem *b, void *aux);
void thread_preempt(void);

/* [P1-3] 헤더 파일 선언 */
void calculate_priority(struct thread *t);
void add_recent_cpu(void);
void recalculate_priority(void);
void recalculate_recent_cpu(void);
void recalculate_load_avg(void);

struct thread* thread_get_highest_priority();
struct thread* thread_get_by_tid(tid_t tid);

#endif /* threads/thread.h */
