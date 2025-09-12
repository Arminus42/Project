#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/fat.h"
#include "threads/malloc.h"
#include "lib/string.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
	dir_set_pwd(dir_open_root());	// [P4-2] pwd를 root로 설정
	// struct dir* root = dir_open_root();
	// msg("[filesys init] rood dir: %d",inode_get_inumber(dir_get_inode(root)));
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

struct dir* get_working_dir(char *_path, char *_name, bool deep_search);

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
	if(name == NULL || *name == '\0') return NULL; // [P4-1] 잘못된 파일 이름인 경우 종료
// msg("[filesys create] name: %s", name);
#ifndef EFILESYS
	disk_sector_t inode_sector = 0;
	struct dir *dir = dir_open_root ();
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
	dir_close (dir);

	return success;
#else // [P4-1] FAT 기반 구현
	/* [P4-2] 경로로 create 접근 가능하게 수정 */
	char* file_name;
	file_name = (char*)malloc((strlen(name)+1)*sizeof(char));

	struct dir *dir = get_working_dir(name, file_name, true); // [P4-1] 루트 디렉토리 열기, [P4-2] 기존 root에서 경로로 기반으로 변경
	if(dir == NULL) return false;

	// msg("[create file] current dir: %d", inode_get_inumber(dir_get_inode(dir)));

	// msg("[filesys create] check dir %d", inode_get_inumber(dir_get_inode(dir)));

	cluster_t clst = fat_create_chain(0); // [P4-1] 새로운 FAT 체인 할당
    if (clst == 0) {	// [P4-1] 할당할 FAT 없으면 취소
        dir_close(dir);
		free(file_name);
        return false;
    }
    disk_sector_t inode_sector = cluster_to_sector(clst); // [P4-1] 해당 클러스를 sector로 변환

	if(!inode_create(inode_sector, initial_size, 0)){ // [P4-1] inode 생성
		fat_remove_chain(clst, 0);
		dir_close(dir);
		free(file_name);
		return false;
	}


	// msg("[create file] add %s(%d) to dir %d", file_name, inode_sector, inode_get_inumber(dir_get_inode(dir)));

	if(!dir_add(dir, file_name, inode_sector)){ // [P4-1] 디렉토리 엔트리에 파일 이름과 inode 연결
		inode_remove(inode_open(inode_sector));
		fat_remove_chain(clst, 0);
		dir_close(dir);
		free(file_name);
		// msg("[filesys create] fail");
		return false;
	}

	// char test[15] = "";
	// msg("[filesys create] dir: %d", inode_get_inumber(dir_get_inode(dir)));
	// while(dir_readdir(dir, test))
	// 	msg("[filesys change dir] read dir: %s", test);

	dir_close (dir); // [P4-1] 디렉토리 해제
	free(file_name);
	return true; // [P4-1] 성공 여부 반환
#endif
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
//#ifndef EFILESYS
	if(name == NULL || *name == '\0') return NULL; // [P4-1] 잘못된 파일 이름인 경우 종료
	// msg("[filesys open] open %s", name);
	char* file_name;
	file_name = (char*)malloc((strlen(name)+1)*sizeof(char));

	struct dir *dir = get_working_dir(name, file_name, true);  // [P4-2] 기존 root에서 경로 기반으로 변경
	struct inode *inode = NULL;
	bool success;

	// msg("[filesys open] current dir: %d", inode_get_inumber(dir_get_inode(dir)));
	// msg("[filesys open] file name: %s", file_name);

	if (dir != NULL)
		success = dir_lookup (dir, file_name, &inode);

	// msg("[file_open] opened file: %d", inode_get_inumber(inode));
	dir_close (dir);
	free(file_name);
	return file_open (inode);
//#else // [P4-1] FAT 기반 구현
//#endif
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
	if(name == NULL || *name == '\0') return NULL; // [P4-1] 잘못된 파일 이름인 경우 종료
	char* file_name;
	file_name = (char*)malloc((strlen(name)+1)*sizeof(char));
	bool success = true;
	// msg("[filesys remove] try remove %s", name);
	struct dir *dir = get_working_dir(name, file_name, false);  // [P4-2] 기존 root에서 pwd 기반으로 변경
	// msg("[filesys remove] target name %s", file_name);
	struct inode *inode;
	if (dir != NULL){
		/* dir 내에서 지우려고 하는 inode 확인 */
		if(!dir_lookup (dir, file_name, &inode)){
			free(file_name);
			dir_close(dir);
			return false;
		}

		if(dir_is_dir(inode)){
			/* 지우는 대상이 dir인 경우 */
			if(inode_get_open_cnt(inode) > 1){
				/* 이미 열려있는 폴더면 삭제 불가 */
				// msg("[filesys remove] oppened inode %d, %d", inode_get_inumber(inode), inode_get_open_cnt(inode));
				success = false;
			}
			else{
				// msg("[filesys remove] remove dir");
				struct dir* target_dir = dir_open(inode);
				char name[15] = "";
				while(dir_readdir(target_dir, name)){
					if(strcmp(name, ".") != 0 && strcmp(name, "..") != 0){
						/* 지우려는 폴더 안에 다른 파일이 있는 경우 삭제 불가 */
						success = false;
						break;
					}
				}

				// msg("[filesys remove] dir is empty: %d", success);

				struct dir* pwd = dir_get_pwd();
				if(success && (inode_get_inumber(inode) != inode_get_inumber(dir_get_inode(pwd)))){
					/* 폴더 내부에 다른 파일이 없고, 해당 폴더가 pwd가 아닌 경우 삭제 가능 */
					// msg("[filesys remove] dir is not pwd: target %d, pwd %d", inode_get_inumber(inode), inode_get_inumber(dir_get_inode(pwd)));
					success = dir_remove(dir, file_name);
				} else {
					success = false;
				}
				dir_close(pwd);
				dir_close(target_dir);
			}
		}
		else{
			/* 지우는 대상이 파일인 경우 그냥 삭제(열려있어도 dir_remove에서 삭제 대기기) */
			// msg("[filesys remove] remove file");
			inode_remove(inode);
			success = dir_remove(dir, file_name);
		}
	}

	inode_close(inode);
	dir_close (dir);
	free(file_name);
	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS // [P4-1] FAT 기반 구현
	/* Create FAT and save it to the disk. */
	fat_create ();
	disk_sector_t root = cluster_to_sector(ROOT_DIR_CLUSTER);
    if (!dir_create(root, 16))
        PANIC("root directory creation failed");
	
	/* [P4-2] root directory의 current dir와 prev dir를 자기 자신으로 설정 */
	dir_add(dir_open_root(), ".", root);
	dir_add(dir_open_root(), "..", root);
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

/* [P4-2] dir syscall helper function */

bool
filesys_create_dir(const char* dir){
	// msg("[filesys create dir] invoke");
	if(dir == NULL || *dir == '\0') return NULL; // [P4-1] 잘못된 파일 이름인 경우 종료
	/* [P4-2] dir 생성 함수 */
	char* file_name = (char *)calloc(strlen(dir)+1, sizeof(char));
	char* name; 
	
	if(file_name == NULL)
		return false;

	/* dir 생성해야 하는 위치 찾기 */
	struct dir* cur_dir = get_working_dir(dir, file_name, true);
	struct inode* inode;

	// msg("[create dir] name: %s", file_name);
	cluster_t clst = fat_create_chain(0); // [P4-2] 새로운 FAT 체인 할당
	if (clst == 0) {	// [P4-2] 할당할 FAT 없으면 취소
		dir_close(cur_dir);
		free(file_name);
		return false;
	}
	disk_sector_t inode_sector = cluster_to_sector(clst); // [P4-2] 해당 클러스를 sector로 변환

	if(!dir_create(inode_sector, 16)){ // [P4-2]dir inode 생성
		fat_remove_chain(clst, 0);
		dir_close(cur_dir);
		free(file_name);
		return false;
	}

	/* dir 추가 후 새로 생성한 dir 진입 */
	// msg("[create dir] current dir: %d", inode_get_inumber(dir_get_inode(cur_dir)));
	if(!dir_add(cur_dir, file_name, inode_sector) || !dir_lookup(cur_dir, file_name, &inode)){ // [P4-2] 디렉토리 엔트리에 파일 이름과 inode 연결
		inode_remove(inode_open(inode_sector));
		fat_remove_chain(clst, 0);
		dir_close(cur_dir);
		free(file_name);
		return false;
	}

	/* 새로 생성한 dir에에 "."과 ".."  설정*/
	struct dir* new_dir = dir_open(inode);
	// msg("[create dir] new dir %s: %d", file_name, inode_get_inumber(dir_get_inode(new_dir)));
	if(!dir_add(new_dir, ".", inode_sector)
		|| !dir_add(new_dir, "..", inode_get_inumber(dir_get_inode(cur_dir)))){
		inode_remove(inode_open(inode_sector));
		fat_remove_chain(clst, 0);
		dir_close(cur_dir);
		dir_close(new_dir);
		free(file_name);
		return false;
	}

	dir_close (cur_dir); // [P4-2] 디렉토리 해제
	dir_close(new_dir);
	free(file_name);
	return true;
}

bool filesys_change_dir(const char *dir){
	/* [P4-2] dir 생성 함수 */
	// msg("[filesys change dir] invoke");
	char* path = (char *)calloc(strlen(dir)+1, sizeof(char));

	/* Abs path -> root에서부터 시작, Rel path: pwd에서부터 시작 */
	struct dir* cur_dir = get_working_dir(dir, path, true);

	struct inode* inode;
	if(!dir_lookup(cur_dir, path, &inode)){
		dir_close(cur_dir);
		free(path);
		return false;
	}

	struct dir* new_dir = dir_open(inode);
	// msg("[filesys change dir] change dir to: %d", inode_get_inumber(inode));

	// char test[15] = "";
	// while(dir_readdir(new_dir, test))
	// 	msg("[filesys change dir] read dir: %s", test);

	if(new_dir == NULL){
		dir_close(cur_dir);
		dir_close(new_dir);
		free(path);
		// msg("[filesys change dir] fail");
		return false;
	}

	dir_set_pwd(new_dir);
	dir_close(cur_dir);

	free(path);
	// msg("[filesys change dir] success");
	return true;
}

bool filesys_is_dir(struct file* file){
	/* filelock 상태로 진입 */
	if(file == NULL) 
		return false;
	
	/* 파일로부터 inode 읽어서 dir inode인지 판단 값 리턴 */
	struct inode* inode = file_get_inode(file);

	if(inode == NULL)
		return false;
	
	return inode_is_dir(inode);
} 

bool filesys_read_dir(struct file* file, char* name){
	if(file == NULL)
		return false;

	struct inode* inode = file_get_inode(file);

	if(inode == NULL)
		return false;

	if(!inode_is_dir(inode))
		/* 해당 파일 위치가 dir이 아니면 실패 */
		return false;

	struct dir* dir = file_get_dir(file);
	// msg("[filesys read dir] dir: %d", inode_get_inumber(inode));

    while (dir_readdir(dir, name)) {
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
            return true;  // 유효한 항목 발견
        // 아니라면 loop 계속 → dir->pos는 자연히 증가
    }

	// msg("[filesys read dir] return false");
	return false;
}

int filesys_inumber(struct file* file){
	if(file == NULL)
		return 0;

	struct inode* inode = file_get_inode(file);

	if(inode == NULL)
		return 0;

	return inode_get_inumber(inode);
}

/* [P-2] Symlink 생성 함수 */
/* linkpath 경로로 바로 접근 가능한 파일 만듦 */
int
filesys_symlink(const char *target, const char *linkpath){
	char* file_name;
	file_name = (char*)calloc(strlen(linkpath)+1, sizeof(char));

	struct dir* dir = get_working_dir(linkpath, file_name, true);	// 생성 위치 확인
	// msg("[filesys symlink] try create link %s", target);
	if(dir == NULL){
		free(file_name);
		return -1;
	}

	cluster_t clst = fat_create_chain(0);	// FAT chain 할당

    if (clst == 0) {	// [P4-1] 할당할 FAT 없으면 취소
        dir_close(dir);
		free(file_name);
        return -1;
    }

    disk_sector_t inode_sector = cluster_to_sector(clst); // [P4-1] 해당 클러스를 sector로 변환

	// msg("[filesys symlink] create link %d", inode_sector);
	if(!inode_create_link(inode_sector, strlen(target)+1, target)){ // [P4-1] inode 생성
		fat_remove_chain(clst, 0);
		dir_close(dir);
		free(file_name);
		return -1;
	}


	// msg("[create file] add %s(%d) to dir %d", file_name, inode_sector, inode_get_inumber(dir_get_inode(dir)));

	if(!dir_add(dir, file_name, inode_sector)){ // [P4-1] 디렉토리 엔트리에 파일 이름과 inode 연결
		inode_remove(inode_open(inode_sector));
		fat_remove_chain(clst, 0);
		dir_close(dir);
		free(file_name);
		// msg("[filesys create] fail");
		return -1;
	}

	return 0;
}

/* [P4-2] 주어진 경로 _path에서 최하위 dir를 찾는 함수, 추가로 최하위 파일의 이름을 name에 저장장 */
/* e.g. create(/a/b/c) => /a/b/ dir에 c를 생성해야 하기 때문에 이 함수는 b dir를 반환 */
struct dir* get_working_dir(char *_path, char *_name, bool deep_search){
	char* path = (char *)calloc(strlen(_path)+1, sizeof(char));
	char* name, *next_name, *save_ptr; 
	
	if(path == NULL)
		return NULL;
	// msg("[get working dir] finding path: %s", _path);

	strlcpy(path, _path, strlen(_path)+1);

	/* Abs path -> root에서부터 시작, Rel path: pwd에서부터 시작 */
	struct dir* cur_dir = path[0] == '/' ? dir_open_root() : dir_get_pwd();
	// msg("[get working dir] current dir: %d", inode_get_inumber(dir_get_inode(cur_dir)));

	name = strtok_r(path, "/", &save_ptr);	// 첫번째 경로, 상대경로인 경우 .이거나 ..

	if(name == NULL){
		/* "/"위치에 접근 => root */
		strlcpy(_name, ".", 2);
		free(path);
		return cur_dir;
	}
	bool is_linking = false;
	struct inode* inode;
	while(1){
		if(!is_linking)
			next_name = strtok_r(NULL, "/", &save_ptr);
		else
			is_linking = false;
		// msg("[get working dir] searching: %s, next: %s", name, next_name);
		if(next_name == NULL){
			/* 현재 name이 가리키는 파일이 link 인 경우*/
			if(deep_search){
				if(dir_lookup(cur_dir, name, &inode)){
					if(inode_is_link(inode)){
						dir_close(cur_dir);
						cur_dir = get_working_dir(inode_get_linkpath(inode), name, deep_search);
					}
					inode_close(inode);
				}
			}
			
			strlcpy(_name, name, strlen(name)+1);
			// msg("[get working dir] found dir %d name: %s", inode_get_inumber(dir_get_inode(cur_dir)), name);
			return cur_dir;
		}
		else{
			/* 아직 폴더 생성할 위치를 찾는 중인 경우 */
			if(!dir_lookup(cur_dir, name, &inode)){
				/* 찾으려는 경로/파일이 현재 dir에 없으면 실패 */
				dir_close(cur_dir);
				free(path);
				return NULL;
			}

			// msg("[get working dir]  %s is link file: %d", name, inode_is_link(inode));
			/* link 포함한 깊은 탐색 필요시 + link 파일일 시 링크 끝까지 탐색 */
			if(inode_is_link(inode)){
				/* link파일인 경우 해당 link 위치에서부터 다시 탐색 */
				dir_close(cur_dir);
				cur_dir = get_working_dir(inode_get_linkpath(inode), name, deep_search);
				is_linking = true;
				inode_close(inode);
				continue;
			}

			// msg("[get working dir] found %s: %d", name, inode_get_inumber(inode));
			if(!inode_is_dir(inode)){
				/* 폴더 생성 위치가 폴더가 아닌 경우 실패 */
				// msg("[get working dir] not a dir: %s", name);
				
				inode_close(inode);
				dir_close(cur_dir);
				free(path);
				return NULL;
			}

			/* 다음 탐색 위치로 이동 */
			dir_close(cur_dir);
			cur_dir = dir_open(inode);

			name = next_name;
		}
	}
}
