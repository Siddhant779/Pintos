#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>

#include "devices/block.h"
#include "filesys/inode.h"

/* Maximum length of a file name component.
 * This is the traditional UNIX maximum length.
 * After directories are implemented, this maximum length may be
 * retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

/* A directory. */
struct dir {
    struct inode *inode; /* Backing store. */
    off_t         pos;   /* Current position. */
    //Pos keeps track of which file dir_lookup is looking at, should be 0 as default, so when you create a dir, set it as zero
    // struct dir *parent;
};

/* A single directory entry. */
struct dir_entry {
    block_sector_t inode_sector;       /* Sector number of header. */
    char           name[NAME_MAX + 1]; /* Null terminated file name. */
    bool           in_use;             /* In use or free? */
};

/* Opening and closing directories. */
bool dir_create(block_sector_t sector, size_t entry_cnt);
struct file *dir_open(struct inode *);
struct file *dir_open_root(void);
struct file *dir_reopen(struct file *);
struct file *dir_sector_open(block_sector_t sector);
struct file *dir_open_current();
void dir_close(struct file *);
struct inode *dir_get_inode(struct file *);

/* Reading and writing. */
bool dir_lookup(const struct file *, const char *name, struct inode **);
struct file* parse_dir(const char* dir);
bool parse_file_name(const char* file, char name[NAME_MAX + 1]);
bool dir_add(struct file *, const char *name, block_sector_t);
bool dir_remove(struct file *, const char *name);
bool dir_readdir(struct file *, char name[NAME_MAX + 1]);

#endif /* filesys/directory.h */
