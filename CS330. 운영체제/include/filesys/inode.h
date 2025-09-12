#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"

struct bitmap;

typedef uint32_t bool32_t;

#define INODE_FILE 0
#define INODE_DIR 1
#define INODE_DEFAULT 0
#define INODE_LINK 1

void inode_init (void);
bool inode_create (disk_sector_t, off_t, bool32_t);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

bool inode_is_dir (struct inode *); /* [P4-2] inode data가 dir인지 확인 */
bool inode_is_link(struct inode *); /* [P4-2] inode가 symlink용인지 확인 */
int inode_get_open_cnt (struct inode *); /* [P4-2] inode가 이미 open 돼있는지 확인 */
bool inode_create_link(disk_sector_t, off_t, const char*); /* [P4-2] symlink용 inode 생성 함수 */
char* inode_get_linkpath(struct inode*); /* [P4-2] link inode의 path 받아오는 함수 */

#endif /* filesys/inode.h */
