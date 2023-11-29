#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <list.h>
#include <stdbool.h>

#include "devices/block.h"
#include "filesys/off_t.h"

struct bitmap;

/* On-disk inode.
 * Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    block_sector_t start;       /* First data sector. */
    //start doesn't tell u where this inode is, just tells you where the data for the file is
    //Ex: inode in sector 37 and data in 52, data points to 52
    //How it works before filesys, it only looks through the root dir, make it look thru each part of directory path
    off_t          length;      /* File size in bytes. */
    unsigned       magic;       /* Magic number. */
    uint32_t       unused[125]; /* Not used. */
    bool directory; /* Inode is directory or file */

    //Store contents of directory in another sector 
    //Each file/directory will contain alot of pointers, make one of the pointers point to the contents of the directory if it is a directory
};

//Store contents of directory in here (when you block read, you access the sector and reads 512 bytes, cast to struct to read the contents)
struct contents {
    //Define what each entry is, have an array of them not exceeding 512 bytes (use dir entry from directory.h)
    struct dir_entry* entries;
};

/* In-memory inode. */
struct inode {
    struct list_elem  elem;           /* Element in inode list. */
    block_sector_t    sector;         /* Sector number of disk location. */
    int               open_cnt;       /* Number of openers. */
    bool              removed;        /* True if deleted, false otherwise. */
    int               deny_write_cnt; /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;           /* Inode content. */
    /* Access bool directory thru inode.data field, that pointd to an inode_disk with that info */
};

void inode_init(void);
bool inode_create(block_sector_t, off_t, bool dir); //Modify to accept param on whether file is dir
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);

#endif /* filesys/inode.h */
