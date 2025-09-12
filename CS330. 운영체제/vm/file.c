/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

static struct lock file_lock;

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

void write_back_if_dirty(struct page *page, struct file_page *file_page){
	if (page->writable && pml4_is_dirty(thread_current()->pml4, page->va)) {
		// P3. page에 write 됐으면 덮어쓰기
		// 파일 포인터 이동 후 write
		// msg("write back: %p", page->frame->kva);
		file_write_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->offset);
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}
}

/* The initializer of file vm */
void
vm_file_init (void) {
	lock_init(&file_lock);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// msg("file_backed_initializer: page %p, page->va %p, type %d, kva %p", page, page->va, type, kva);
	page->operations = &file_ops;
	// msg("file_backed_initializer: page: %p", page);

	struct file_page *file_page = &page->file;
	file_page->file = NULL;
	file_page->offset = 0;
	file_page->read_bytes = 0;
	file_page->zero_bytes = 0;
	
	return true;
}

/* P3. file page에 사용할 lazy load 함수 */
bool
file_backed_init(struct page *page, void *aux){
	// msg("file_backed_init: page %p, page->va %p, aux %p", page, page->va, aux);
	if(aux == NULL)
		return false;
	// msg("file_backed_init: aux: %p", page);

	struct file_page *data = aux;
	struct file_page *file_page = &page->file;
	
	/* P3. 받은 aux data를 file page에 할당(lazy load) */
	file_page->file = data->file;
	file_page->offset = data->offset;
	file_page->read_bytes = data->read_bytes;
	file_page->zero_bytes = data->zero_bytes;

	void* kva = page->frame->kva;

	/* P3. 해당 파일 위치 읽어서 kva에 저장 */
	if(!file_read_at(file_page->file, kva, file_page->read_bytes, file_page->offset)){
		free(data);
		// msg("file_backed_init: file_read_at failed");
		return false;
	}

	/* P3. zero padding (page size align)*/
	memset(kva + file_page->read_bytes, 0, file_page->zero_bytes);
	// msg("file_backed_init: memset done");
	free(data);
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	// msg("file swap in: %p", page->va);
	struct file_page *file_page = &page->file;
	
	/* P3. 파일이 없으면 실패 */
	if (file_page->file == NULL)
		return false;

	/* P3. 파일 포인터 offset 만큼 이동 후 read */
	if(!file_read_at(file_page->file, kva, file_page->read_bytes, file_page->offset)){
		return false;
	}
	
	/* P3. 나머지 부분 zero padding */
	memset(kva + file_page->read_bytes, 0, file_page->zero_bytes);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	// msg("file swap out: %p", page->va);
	struct file_page *file_page = &page->file;
	
	/* P3. 파일이 없으면 실패 */
	if (file_page->file == NULL)
		return false;

	/* P3. 수정 여부 확인 후 write back */
	write_back_if_dirty(page, file_page);

	/* P3. frame 연결 끊기 및 page 초기화 */
	if(page->frame != NULL){
		page->frame->page = NULL;
		page->frame = NULL;
	}

	// palloc_free_page(page->frame->kva);
    // page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	// msg("file_backed_destroy: %p", page->va);
	struct file_page *file_page = &page->file;
	
	if (file_page->file == NULL) return;

	write_back_if_dirty(page, file_page);

	pml4_clear_page(thread_current()->pml4, page->va);

    if (page->frame != NULL) {
		palloc_free_page(page->frame->kva);
        page->frame->kva = NULL;
        page->frame = NULL;
    }
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	/* file lock 걸려있는 상태로 호출 */
	// P3. 현재 thread의 fds에서 탐색한 fd를 file로 입력 받음
	//		=> reopen 가능
	if(is_kernel_vaddr(addr))
		return NULL;

	struct file *reopened_file = file_reopen(file);
	if (reopened_file == NULL) {
		file_close(reopened_file);
		return NULL;
	}

	/* P3. 전체 읽어야 할 byte 수 */
	size_t total_bytes = length % PGSIZE == 0 ? length : PGSIZE * (length / PGSIZE + 1);
	if(offset % PGSIZE != 0 || offset > file_length(reopened_file))
		return NULL;

	// P3. 총 읽어야 할 바이트 수(전체 바이트 수 - offset 바이트)	
	size_t read_bytes = file_length(reopened_file) - offset;

	if (read_bytes > length)
		read_bytes = length;
		
	// P3. zero padding 해야 할 바이트 수 
	size_t zero_bytes = total_bytes - read_bytes;
	if (zero_bytes % PGSIZE == 0)
		zero_bytes = 0;

	// msg("do_mmap: read_bytes: %d, zero_bytes: %d", read_bytes, zero_bytes);
	off_t ofs = offset;
	void *upage = addr;
	/* 페이지 단위로 file page 생성 */
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct segment_aux *aux = malloc(sizeof(struct segment_aux)); // [P3-2] aux 구조체 동적 할당
		
		/* [P3-2] aux에 정보 저장 */
		aux->file = reopened_file;
		aux->offset = ofs;
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;
		aux->writable = writable;   

		// msg("mmap: mapping addr %p", upage);

		if (!vm_alloc_page_with_initializer (VM_FILE, upage,
					writable, file_backed_init, aux)){
			free(aux); // [P3-2] 할당 실패시 aux 메모리 해제
			file_close(reopened_file);
			return NULL;
		}
		// msg("do_mmap: mapped page: va: %p", upage);
		struct page *page = spt_find_page(&thread_current()->spt, upage);
		// msg("do_mmap: mapped page: va: %p, pa: %p", upage, page);
		// msg("do_mmap: read_bytes: %d", read_bytes);
		// msg("do_mmap: zero_bytes: %d", zero_bytes);

		ofs += page_read_bytes; // [P3-2] 다음 파일 오프셋으로 이동

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	// msg("do_mmap: return addr %p", addr);
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct page *page = spt_find_page(&thread_current()->spt, addr);
	if (page == NULL)
		return;

	/* P3. addr 부터 시작되는 file_page들에 대해서 unmap 진행 */
	/* 	   file read byte가 page size보다 크면 연속되는 공간에 여러 page에 걸쳐 memory mapped가 되어있음 */
	while (page != NULL) {
		struct file_page *file_page = &page->file;
		void *next_va = page->va + PGSIZE;
		if (file_page->file == NULL)
			return;

		/* P3. write되었는지 확인 후 disk update */
		write_back_if_dirty(page, file_page);

		/* P3. spt에서 page 제거, destroy */
		spt_remove_page(&thread_current()->spt, page);

		page = spt_find_page(&thread_current()->spt, next_va);
	}
	// msg("do_munmap: write back done");
}
