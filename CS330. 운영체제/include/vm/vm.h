#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include <hash.h>

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "threads/mmu.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */
	struct hash_elem hash_elem; // [P3-2] SPT용 해시 노드
	bool writable; // [P3-2] 페이지 읽기 가능 여부

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
	struct list_elem frame_elem; // [P3-2] 프레임 리스트용
};

/* [P3-2] Swap disk에서 페이지 저장 영역 단위 구조체 */
struct disk_sector{
	struct page *page; 		// [P3-2] Sector의 페이지
	size_t index; 			// [P3-2] Sector의 인덱스 (0 ~ N-1)
	struct list_elem elem;  // [P3-2] Swap table 리스트 추가용 element
};

/* [P3-2] 페이지를 처음 할당할 때 결정한 정보들을 담는 보조 구조체 (load_segment->lazy_load_segment로 전달) */
struct segment_aux {
	struct file *file;			// [P3-2] 읽을 실행 파일
	off_t offset;				// [P3-2] 실행 파일 내의 offset
	size_t page_read_bytes;		// [P3-2] 읽을 바이트 수
	size_t page_zero_bytes;     // [P3-2] 0으로 채울 바이트 수
	bool writable;              // [P3-2] 페이지의 쓰기 가능 여부
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */

struct supplemental_page_table {
	struct hash hash; // [P3-1] 해시 사용
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED); // [P3-2] SPT 해시 함수 
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED); // [P3-2] 페이지 비교 함수
void page_destroy(struct hash_elem *e , void *aux UNUSED); // [P3-2] hash table에서 제거되는 페이지 메모리를 해제하는 함수


#endif  /* VM_VM_H */
