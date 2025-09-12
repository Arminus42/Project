/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* [P3-2] 사용자 정의 변수 */
static struct disk *swap_disk; // [P3-2] Swap 데이터를 저장하는 보조 저장 공간
struct list swap_table; // [P3-2] 익명 페이지 swap용 리스트
struct lock anon_lock; // [P3-2] 익명 페이지 Lock
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {

	// msg("vm_anon_init");
	list_init(&swap_table); // [P3-2] Swap table 초기화
	lock_init(&anon_lock); // [P3-2] 익명 페이지 Lock 초기화

	swap_disk = disk_get(1, 1);  // [P3-2] Swap disk 할당
    ASSERT(swap_disk != NULL); // [P3-2] Swap disk 할당 실패

    disk_sector_t disk_sector_cnt = disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE); // [P3-2] Swap slot 개수 계산
	
	/* [P3-2] 모든 sector를 swap table에 추가 */
	for(int i = 0; i < disk_sector_cnt; i++){
		struct disk_sector *sector = malloc(sizeof(struct disk_sector)); // [P3-2] Sector 할당
		ASSERT(sector != NULL); // [P3-2] Sector 할당 실패
		
		/* Sector 초기화 */
		sector->page = NULL;
		sector->index = i;

		/*[P3-2] Sector를 swap table에 추가 + 공유 자원 Swap_table에 대해 동기화 */
		lock_acquire(&anon_lock);
		list_push_back(&swap_table, &sector->elem);
		lock_release(&anon_lock);
	}

}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// msg("anon_initializer: page->va %p, type %d", page->va, type);
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->slot_index = (disk_sector_t)(-1); // [P3-2] Swap X 표현
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	// msg("try swap in %p", page->va);
	struct anon_page *anon_page = &page->anon;
	// msg("anon swap in: %p", page->va);
	/* P3. non-swap page. return 값 더블체크 필요 */
	if (anon_page->slot_index == (disk_sector_t)(-1))
		return true;

	/* P3. 스왑 디스크에서 페이지 데이터 읽기 */
	for (int i = 0; i < PGSIZE / DISK_SECTOR_SIZE; i++) {
		disk_read(swap_disk, anon_page->slot_index * (PGSIZE / DISK_SECTOR_SIZE) + i, kva + (i * DISK_SECTOR_SIZE));
	}

	/* 스왑 테이블에서 해당 슬롯 해제 */
	lock_acquire(&anon_lock);
	struct list_elem *e;
	for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)) {
		struct disk_sector *sector = list_entry(e, struct disk_sector, elem);
		if (sector->index == anon_page->slot_index) {
			sector->page = NULL;
			break;
		}
	}
	lock_release(&anon_lock);

	/* 스왑 슬롯 인덱스 초기화 */
	anon_page->slot_index = (disk_sector_t)(-1);
	// msg("success swap in %p", page->va);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	// msg("try swap out %p", page->va);
	struct anon_page *anon_page = &page->anon;
	// msg("anon swap out: %p", page->va);
	bool is_empty = true;

	/* P3. 빈 disk sector 찾기 */
	lock_acquire(&anon_lock);
	struct list_elem *e;
	struct disk_sector *sector = NULL;

	if(list_empty(&swap_table)){
		lock_release(&anon_lock);
		return false;
	}
	
	for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)) {
		sector = list_entry(e, struct disk_sector, elem);
		if (sector->page == NULL) {
			sector->page = page;
			anon_page->slot_index = sector->index;
			is_empty = false;
			break;
		}
	}
	lock_release(&anon_lock);

	/* P3. 빈 disk sector 없으면 false return */
	if (is_empty)
		return false;

	/* P3. DISK_SECTOR_SIZE 단위로 데이터 write */
	for (int i = 0; i < PGSIZE / DISK_SECTOR_SIZE; i++) {
		disk_write(swap_disk, anon_page->slot_index * (PGSIZE / DISK_SECTOR_SIZE) + i, page->va + (i * DISK_SECTOR_SIZE));
	}

	/* P3. write 끝났으니 할당된 page clear */
	pml4_clear_page(thread_current()->pml4, page->va);

	/* P3. frame-page 연결 끊기 */
	if(page->frame != NULL){
		page->frame->page = NULL;
		page->frame = NULL;
	}

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	// msg("anon_destroy: page->va %p", page->va);
	struct anon_page *anon_page = &page->anon;

	/* P3. page clear */
	pml4_clear_page(thread_current()->pml4, page->va);

	/* P3. frame 연결 끊기 및 free */
	if(page->frame != NULL){
		page->frame->page = NULL;     
		palloc_free_page(page->frame->kva);
		page->frame->kva = NULL;
		page->frame = NULL;           
	}
}
