/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "vm/anon.h"

struct list frame_table; // [P3-2] 전역 프레임 테이블 선언

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table); // [P3-2] 전역 프레임 테이블 초기화
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	struct page *p = NULL; // [P3-2] 할당받을 페이지 변수 선언

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
		p = (struct page *)malloc(sizeof(struct page)); // [P3-2] 새로운 페이지 할당
		if(p == NULL) return false; // [P3-2] 페이지 할당 실패 시 false 반환

		/* [P3-2] VM_TYPE에 따른 initializer 선택 */
		vm_initializer *inner_init = NULL;
		switch (VM_TYPE(type)){
			case VM_ANON:
				inner_init = anon_initializer;
				break;
			case VM_FILE:
				inner_init = file_backed_initializer;
				break;
			default: // [P3-2] 다른 VM 타입인 경우
				PANIC("UNKNOWN VM_TYPE");
				goto err;
		}
		uninit_new(p, upage, init, type, aux, inner_init); // [P3-2] uninit 페이지로 초기화
		p->writable = writable; // [P3-2] 새 페이지의 writable 속성 설정
		if(!spt_insert_page(spt, p)) goto err; // [P3-2] SPT에 페이지 삽입

		return true;
	}
err: // [P3-2] 오류 발생시 페이지 메모리 해제 후 false 반환
	free(p);
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* [P3-2] SPT에서 페이지 VA 찾기 */

	struct page p;
    struct hash_elem *e;

    p.va = pg_round_down(va);  // [P3-2] 페이지의 시작 주소로 VA 맞추기
    e = hash_find(&spt->hash, &p.hash_elem);

	if(e == NULL) return NULL; // [P3-2] 페이지를 못 찾은 경우
	else return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	/* [P3-2] SPT에서 페이지 삽입 */

	if(hash_insert(&spt->hash, &page->hash_elem) == NULL) return true; // [P3-2] 삽입 성공시 hash_insert()가 NULL 반환
	else return false;

}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->hash, &page->hash_elem);  // [P3-2] SPT에서 페이지 제거
	vm_dealloc_page(page);  // [P3-2] 페이지 할당 해제
	// TODO: frame 관련 처리?
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	/* [P3-1] 교체할 프레임을 선택하는 함수 */

	if(list_empty(&frame_table)) return NULL;  // [P3-1] 프레임 테이블 빈 경우 제외

	struct frame *victim = NULL;
	struct list_elem *e = list_head (&frame_table);
	/* P3. frame 중에서 stack과 관련된 frame(VM_MARKER_0)는 제외하고 evict 할 frame 선택 */
	while ((e = list_next (e)) != list_end (&frame_table))
	{
		struct frame *frame = list_entry(e, struct frame, frame_elem);
		if(frame->page->uninit.type & VM_MARKER_0) 
			continue;
		else {
			list_remove(e);
			victim = frame;
			break;
		}
	}

    // struct list_elem *e = list_pop_front(&frame_table);  // [P3-1] 가장 오래된 frame 선택 (FIFO 방식)
    // struct frame *victim = list_entry(e, struct frame, frame_elem);

    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	if (victim == NULL || victim->page == NULL) return NULL; // [P3-2] 프레임이 없거나 빈 프레임이면 NULL 반환

	struct page *page = victim->page;

	/* [P3-2] 페이지가 존재하면 연결 해제 */
	if(page != NULL){ 
		page->frame = NULL; 
		victim->page = NULL;
	}

	list_remove(&victim->frame_elem); // [P3-2] frame_table에서 제거

	swap_out(page);	// P3. swap out
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	/* [P3-2] User pool에서 새로운 Physical Page를 가져오는 함수 */
	void *kva = palloc_get_page(PAL_USER | PAL_ZERO); // [P3-2] User pool에서 0으로 초기화된 페이지 할당
	struct frame *frame = malloc(sizeof *frame); // [P3-2] 프레임 할당

	// msg("get frame: frame %p, kva %p", frame, kva);
    if(frame == NULL) PANIC("Failed to allocate struct frame"); // [P3-2] 프레임 할당 실패
	
    if(kva == NULL){  // [P3-2] 페이지 할당이 실패한 경우
		frame = vm_evict_frame(); // [P3-2] 프레임 교체 및 정리
		ASSERT(frame != NULL); // [P3-2] evict 실패 시 오류
		// msg("evict frame: frame %p, kva %p", frame, frame->kva);
		kva = frame->kva; // [P3-2] victim의 물리 주소 재사용
	}

	/* [P3-2] 프레임 구조체 초기화 */
	frame->kva = kva;
	frame->page = NULL;
	
    list_push_back(&frame_table, &frame->frame_elem); // 4. 프레임 테이블에 등록 (FIFO 정책을 위해 리스트에 삽입)

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
	void *fault_addr = pg_round_down(addr);  // [P3-3] fault가 발생한 주소를 페이지 기준 내림

	/* [P3-3] fault가 난 주소부터 USER STACK까지 한 페이지씩 확장 */
	for(void *p = fault_addr; p < USER_STACK; p += PGSIZE){
		if(p < USER_STACK - (1 << 20)) break; // [P3-3] 1MB 스택 제한을 넘는 경우 확장 중단

		if(spt_find_page(&thread_current()->spt, p) == NULL){ // [P3-3] 해당 주소에 페이지가 존재하지 않는 경우에만 할당
			if(vm_alloc_page_with_initializer(VM_ANON | VM_MARKER_0, p, true, NULL, NULL)){ // [P3-3] anonymous + stack 마커 플래그 설정
				vm_claim_page(p); // [P3-3] 바로 클레임
			}
		}
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
    bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	if(addr == NULL || is_kernel_vaddr(addr)) return false; // [P3-3] kernel 영역이거나 주소가 NULL인 경우 false 반환

	if(!not_present){ // [P3-3] 존재하는 페이지에 접근하는 경우
		page = spt_find_page(&thread_current()->spt, addr);
		if (page == NULL || (write && !page->writable)) return false; // [P3-3] 페이지를 찾을 수 없거나, 쓰기 권한이 없는 경우
		return vm_do_claim_page(page); // [P3-3] 페이지 클레임 성공 여부 반환
	}
	else{ // [P3-3] 존재하지 않는 페이지에 접근하는 경우 (lazy allocation이나 stack growth)
		void *rsp = user ? f->rsp : thread_current()->user_rsp; // [P3-3] rsp 추적 (Page fault 발생 영역: user - f->rsp / kernel - user_rsp)
			
		// msg("try handle fault: %p, rsp: %p, user: %d", addr, rsp, user);
		// [P3-3] Stack growth heuristic:									
		// 1. Fault address가 rsp보다 최대 8byte 아래까지 허용 				
		// 2. Stack 주소 < USER_STACK 주소 							 	
		// 3. Stack 크기 <= 1MB (USER_STACK-(2^20)보다 작은 주소 금지)
		if(addr >= rsp - 8 && addr < USER_STACK && addr >= USER_STACK - (1 << 20)){
			vm_stack_growth(addr); // [P3-3] Stack growth 수행
			return true;
		}
		
		page = spt_find_page(spt, addr); // [P3-3] 방금 growth한 페이지 구조체를 SPT에서 찾기
		if(page == NULL) return false; // [P3-3] 페이지 찾기 실패시 false 반환
		if(write && !page->writable) return false; // 페이지에 쓰려는데 쓰기 권한이 없는 경우 false 반환

		return vm_do_claim_page(page); // [P3-3] 페이지 클레임 성공 여부 반환
	}
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	/* [P3-2] VA에 할당된 페이지를 요청하는 함수 */

	struct page *page = spt_find_page(&thread_current ()->spt, va); // [P3-2] 페이지 할당
    if(page == NULL) return false; // [P3-2] 해당 주소에 대한 페이지가 없는 경우 false
    return vm_do_claim_page(page); // [P3-2] 실제 할당 수행
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	// msg("vm_do_claim_page: frame %p", frame);
	if(frame == NULL) return false; // [P3-2] 프레임 할당 실패시 false 반환
	if(page->frame != NULL) return false; // [P3-2] 이미 프레임이 할당된 페이지인 경우 false 반환
	/* Set links */
	frame->page = page;
	page->frame = frame;

	if(!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) return false; // [P3-2] 페이지 테이블 매핑 실패시 false 반환

	// msg("vm_do_claim_page: swap_in type %d", page->operations->type);
	return swap_in(page, frame->kva); // [P3-2] 가상 페이지 데이터를 물리 프레임에 저장
}

/* [P3-2] SPT 해시 함수 */
uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED){
    const struct page *p = hash_entry(e, struct page, hash_elem);
    return hash_bytes(&p->va, sizeof p->va);
}

/* [P3-2] 페이지 비교 함수 */
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    const struct page *pa = hash_entry(a, struct page, hash_elem);
    const struct page *pb = hash_entry(b, struct page, hash_elem);
    return pa->va < pb->va;
}


/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash, page_hash, page_less, NULL); // [P3-2] SPT 해시로 초기화
}

/* Copy supplemental page table from src to	 dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;

	hash_first(&i, &src->hash);  // [P3-2] src 해시 테이블 순회 시작

	while(hash_next(&i)){
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);  // [P3-2] 현재 페이지 entry 가져오기
		enum vm_type type = src_page->operations->type;
		void *va = src_page->va;
		bool writable = src_page->writable;

		/* [P3-2] lazy-load를 위한 uninit 페이지 복사 */
		if(type == VM_UNINIT){
			struct uninit_page *u = &src_page->uninit;
			
			/* [P3-2] VM_ANON이나 VM_FILE인 경우에만 복사 */
			if(VM_TYPE(u->type) == VM_ANON || VM_TYPE(u->type) == VM_FILE){
				struct segment_aux *src_aux = u->aux;
		
				/* [P3-2] lazy exec/file segment인 경우 aux 구조체 깊은 복사 */
				if(src_aux != NULL && src_aux->file != NULL){
					struct segment_aux *dst_aux = malloc(sizeof(struct segment_aux)); // [P3-2] 새 aux 구조체 할당
					if(dst_aux == NULL) return false; // [P3-2] 메모리 할당 실패시 false 반환
		
					dst_aux->file = file_reopen(src_aux->file); // [P3-2] file 포인터를 복사해 독립적으로 참조
					if(dst_aux->file == NULL){ // [P3-2] reopen 실패 시 메모리 해제 후 false 반환
						free(dst_aux);
						return false;
					}
					
					/* [P3-2] 나머지 필드값 복사 */
					dst_aux->offset = src_aux->offset;
					dst_aux->page_read_bytes = src_aux->page_read_bytes;
					dst_aux->page_zero_bytes = src_aux->page_zero_bytes;
					dst_aux->writable = src_aux->writable;
					
					/* [P3-2] 자식 SPT에 복사된 페이지 등록 후 aux 구조체 메모리 해제 */
					if(!vm_alloc_page_with_initializer(u->type, va, writable, u->init, dst_aux)){
						free(dst_aux);
						return false;
					}
				}
				/* [P3-2] 일반 lazy anonymous page인 경우 aux 없이 할당 */
				else{
					if(!vm_alloc_page_with_initializer(u->type, va, writable, u->init, NULL)) return false;
				}
			}
			/* [P3-2] VM_ANON이나 VM_FILE이 아닌 경우 */
			else PANIC("Unknown UNINIT type in spt_copy");
		}
 		/* [P3-2] 이미 초기화된 페이지의 경우 바로 복사 */
		else{
			if (!vm_alloc_page_with_initializer(type, va, writable, NULL, NULL)) return false; // [P3-2] 페이지 등록 후 실패시 false 반환
			if (!vm_claim_page(va))	return false; // [P3-2] 실제 물리 메모리 할당 및 페이지 테이블에 매핑 후 실패시 false 반환

			struct page *dst_page = spt_find_page(dst, va);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE); // [P3-2] 부모의 메모리 내용을 자식으로 복사
		}
	}
	return true; // [P3-2] 전체 복사 성공시 true 반환
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash, page_destroy);
}

/* [P3-2] hash table에서 제거되는 페이지 메모리를 해제하는 함수 */
void
page_destroy(struct hash_elem *e , void *aux UNUSED){
	struct page *page = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(page);
}