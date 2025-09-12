#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/fat.h" // [P4-1] FAT 기반 구현
#include "lib/string.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
#ifndef EFILESYS
	disk_sector_t start;                /* First data sector. */
#else
	cluster_t start;  // [P4-1] FAT에서는 첫번째 클러스터 번호
#endif
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	bool32_t is_dir;					/* [P4-2] inode가 file용인지 folder용인지 확인, 0:file, 1:folder */
#ifndef EFILESYS
	uint32_t unused[124];               /* Not used. */
#else
	bool32_t is_link;					/* [P4-2] symlink로 생성된 inode인지 확인, 0: 일반 파일, 1: link 파일 */
	char linkpath[492];					/* [P4-2] symlink path 저장 공간 */
#endif

};

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	disk_sector_t sector;               /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) {
	ASSERT (inode != NULL);

#ifndef EFILESYS
	if (pos < inode->data.length)
		return inode->data.start + pos / DISK_SECTOR_SIZE;
	else
		return -1;

#else // [P4-1] FAT 기반 구현
	cluster_t clst = inode->data.start; // [P4-1] 시작 클러스터 번호

	if(pos >= inode->data.length) return -1; // 파일 크기보다 큰 offset인 경우 실패 반환
	if(clst < 2) return -1; // [P4-1] 유효하지 않은 클러스터 번호인 경우 실패 반환

	size_t skip = pos / DISK_SECTOR_SIZE; // [P4-1] pos까지의 섹터 수

	while(skip-- > 0){
		clst = fat_get(clst);
		if(clst == EOChain || clst == 0) return -1; // [@@] 클러스터 할당 실패거나 EOChain이면 실패(-1) 반환
	}

	return cluster_to_sector(clst); // 해당 클러스터를 디스크 섹터 번호로 변환 후 반환
#endif
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool32_t is_dir) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
#ifndef EFILESYS
	if (disk_inode != NULL) {
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		if (free_map_allocate (sectors, &disk_inode->start)) {
			disk_write (filesys_disk, sector, disk_inode);
			if (sectors > 0) {
				static char zeros[DISK_SECTOR_SIZE];
				size_t i;

				for (i = 0; i < sectors; i++) 
					disk_write (filesys_disk, disk_inode->start + i, zeros); 
			}
			success = true; 
		} 
		free (disk_inode);
	}
	return success;

#else // [P4-1] FAT 기반 구현
	if(disk_inode != NULL){
		// msg("[inode create] sector: %d", sector);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		size_t sectors = bytes_to_sectors(length);

		cluster_t first_clst = 0;
		cluster_t prev_clst = 0;

		/* [P4-1] FAT chain 생성 및 클러스터 할당 */
		for(size_t i = 0; i < sectors; i++){
			cluster_t new_clst = fat_create_chain(prev_clst); // [P4-1] 이전 클러스터 뒤에 새 클러스터 생성
			
			/* [P4-1] 클러스터 할당 실패 시 모든 chain을 전부 해제 후 실패 반환 */
			if(new_clst == 0){
				if(first_clst != 0) fat_remove_chain(first_clst, 0);
				free(disk_inode);
				return false;
			}

			if(i == 0) first_clst = new_clst; // [P4-1] 첫 번째 클러스터 저장 (inode_disk->start에 기록용)
			prev_clst = new_clst;
		}

		disk_inode->start = first_clst; // [P4-1] inode_disk에 첫 클러스터 저장

		/* [P4-1] 할당된 클러스터들 0으로 초기화 */
		static char zeros[DISK_SECTOR_SIZE];
		cluster_t clst = first_clst;
		for(size_t i = 0; i < sectors; i++){
			disk_write(filesys_disk, cluster_to_sector(clst), zeros);
			clst = fat_get(clst);
		}
		disk_inode->is_dir = is_dir;
		disk_write(filesys_disk, sector, disk_inode); // [P4-1] inode_disk 구조체를 inode sector에 저장
		success = true; // [P4-1] 여기까지 온 경우 성공 반환
		free(disk_inode); // [P4-1] inode_disk 구조체 메모리 해제
	}
	return success;
#endif
}

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) {
			inode_reopen (inode);
			return inode; 
		}
	}

	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	// if(sector == 318) msg("open");
	disk_read (filesys_disk, inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode) {
	// if(inode->sector == 318) msg("open");
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode) {
	return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) {
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	// if(inode->sector == 318) msg("close");
	/* Release resources if this was the last opener. */
	// msg("[inode close] inode %d open cnt: %d", inode->sector, inode->open_cnt);
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);

#ifndef EFILESYS
		/* Deallocate blocks if removed. */
		if (inode->removed) {
			free_map_release (inode->sector, 1);
			free_map_release (inode->data.start,
					bytes_to_sectors (inode->data.length)); 
		}
#else // [P4-1] FAT 기반 구현
		if(inode->removed){
			// free_map_release (inode->sector, 1);
			cluster_t clst = inode->data.start; // [P4-1]
			if (clst != 0) fat_remove_chain(clst, 0); // [P4-1]
			//TODO: fat_remove_chain(inode->sector, 0) ?
		}	
#endif

		free (inode); 
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove (struct inode *inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	// msg("[inode read at] read at inode: %d, size: %d", inode->sector, size);
	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, sector_idx, bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free (bounce);

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;
	// msg("[inode write at] write at inode: %d, deny: %d", inode->sector, inode->deny_write_cnt);
	if (inode->deny_write_cnt)
		return 0;
		
#ifdef EFILESYS // [P4-1] FAT 기반 구현
		off_t end_pos = offset + size; // [P4-1] 쓰기를 시도하는 파일의 최종 위치
		// msg("[inode write at] write at inode: %d, initial length %d", inode->sector, inode->data.length);

		if(end_pos > inode->data.length){ // [P4-1] file growth 구현
			size_t old_sectors = bytes_to_sectors(inode->data.length);
			size_t new_sectors = bytes_to_sectors(end_pos);
			size_t cnt = new_sectors > old_sectors ? new_sectors - old_sectors : 0; // [P4-1] 추가로 필요한 클러스터 수 계산
	
			cluster_t clst = inode->data.start; // [P4-1] FAT chain의 시작 클러스터
			if(clst == 0 && cnt > 0){ // [P4-1] 파일이 아직 비어 있고 새로운 클러스터가 필요한 경우
				clst = fat_create_chain(0); // [P4-1] 새로운 클러스터 체인 생성
				// msg("[inode write at] new clst: %d", clst);
				if(clst == 0) return 0; // [P4-1] 클러스터 할당 실패
				inode->data.start = clst; // [P4-1] 새 클러스터 inode에 저장
				cnt--;
			}

			while(cnt > 0 && fat_get(clst) != EOChain) clst = fat_get(clst); // FAT 체인의 마지막 클러스터 탐색

			for(size_t i = 0; i < cnt; i++){ // [P4-1] 클러스터 추가 연결
				cluster_t new_clst = fat_create_chain(clst);  // [P4-1] 현재 클러스터 뒤에 새 클러스터 연결
				// msg("[inode write at] chain clst: %d", new_clst);
				if(new_clst == 0) return 0; // [P4-1] 클러스터 할당 실패
				clst = new_clst;
			}
			inode->data.length = end_pos;  // [P4-1] 파일 길이 업데이트

			disk_write(filesys_disk, inode->sector, &inode->data); // [P4-1] inode_disk 데이터 저장
		}
#else
		// [P4-1] 쓰기 범위가 기존 길이를 초과하면 실패
		if(offset + size > inode->data.length)
			return 0;
#endif
	// msg("[inode write at] write at inode: %d, check length %d", inode->sector, inode->data.length);

	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
		} else {
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				disk_read (filesys_disk, sector_idx, bounce);
			else
				memset (bounce, 0, DISK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write (filesys_disk, sector_idx, bounce); 
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free (bounce);
	// msg("[inode write at] writing byte: %d", bytes_written);
	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
	void
inode_deny_write (struct inode *inode) 
{
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode) {
	return inode->data.length;
}

bool
inode_is_dir(struct inode* inode){
	ASSERT(inode != NULL);
	return inode->data.is_dir;
}

int
inode_get_open_cnt(struct inode* inode){
	ASSERT(inode != NULL);
	return inode->open_cnt;
}

 bool
 inode_is_link(struct inode* inode){
	ASSERT(inode != NULL);
	return inode->data.is_link == INODE_LINK;
 }

 bool
 inode_create_link(disk_sector_t sector, off_t length, const char* linkpath){
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);

	if(disk_inode != NULL){
		// msg("[inode create] sector: %d", sector);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
		size_t sectors = bytes_to_sectors(length);

		cluster_t first_clst = 0;
		cluster_t prev_clst = 0;

		/* [P4-1] FAT chain 생성 및 클러스터 할당 */
		for(size_t i = 0; i < sectors; i++){
			cluster_t new_clst = fat_create_chain(prev_clst); // [P4-1] 이전 클러스터 뒤에 새 클러스터 생성
			
			/* [P4-1] 클러스터 할당 실패 시 모든 chain을 전부 해제 후 실패 반환 */
			if(new_clst == 0){
				if(first_clst != 0) fat_remove_chain(first_clst, 0);
				free(disk_inode);
				return false;
			}

			if(i == 0) first_clst = new_clst; // [P4-1] 첫 번째 클러스터 저장 (inode_disk->start에 기록용)
			prev_clst = new_clst;
		}

		disk_inode->start = first_clst; // [P4-1] inode_disk에 첫 클러스터 저장

		/* [P4-1] 할당된 클러스터들 0으로 초기화 */
		static char zeros[DISK_SECTOR_SIZE];
		cluster_t clst = first_clst;
		for(size_t i = 0; i < sectors; i++){
			disk_write(filesys_disk, cluster_to_sector(clst), zeros);
			clst = fat_get(clst);
		}
		disk_inode->is_dir = INODE_FILE;
		disk_inode->is_link = INODE_LINK;	/*[P4-2] link용 inode 마커 추가 */
		strlcpy(disk_inode->linkpath, linkpath, strlen(linkpath)+1); /*[P4-2] 이 link가 가리켜야 하는 path 복사 */
		disk_write(filesys_disk, sector, disk_inode); // [P4-1] inode_disk 구조체를 inode sector에 저장
		success = true; // [P4-1] 여기까지 온 경우 성공 반환
		free(disk_inode); // [P4-1] inode_disk 구조체 메모리 해제
	}
	return success;
 }

 char*
 inode_get_linkpath(struct inode* inode){
	ASSERT(inode != NULL);
	ASSERT(inode->data.is_link);

	return inode->data.linkpath;
 }
