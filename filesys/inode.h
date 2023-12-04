#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <list.h>
#include <stdbool.h>

#include "devices/block.h"
#include "filesys/off_t.h"

struct bitmap;
#define TOTAL_BLOCKS_SIZE 121
#define DIRECT_BLOCKS 119
#define INDIRECT_BLOCKS 1
#define DOUBLE_INDIRECT_BLOCKS 1

#define DIRECT_INDEX 0
#define INDIRECT_INDEX 119
#define DOUBLE_INDIRECT_INDEX 120

#define ROOT_DIR_INDEX 1


/* On-disk inode.
 * Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// 512/4 is 128 


struct inode_disk {
    block_sector_t start;       /* First data sector. */
    //start doesn't tell u where this inode is, just tells you where the data for the file is
    //Ex: inode in sector 37 and data in 52, data points to 52
    //How it works before filesys, it only looks through the root dir, make it look thru each part of directory path
    off_t          length;      /* File size in bytes. */
    unsigned       magic;       /* Magic number. */
    uint32_t       direct_index;
    uint32_t       indirect_index;
    uint32_t       double_indirect_index;
    block_sector_t block_array[TOTAL_BLOCKS_SIZE];

    bool directory; /* Inode is directory or file */

    //we could just store the struct dir pointer 
    //struct dir* entry;

    //struct contents* dirContents; //TODO: Figure out how to implement this in the unused part instead of a separate pointer - WHAT IS THIS FOR?

    //Store contents of directory in another sector 
    //Each file/directory will contain alot of pointers, make one of the pointers point to the contents of the directory if it is a directory
};

//Store contents of directory in here (when you block read, you access the sector and reads 512 bytes, cast to struct to read the contents)
// struct contents {
//     //Define what each entry is, have an array of them not exceeding 512 bytes (use dir entry from directory.h)
//     struct dir* entry;
// };

struct contents {
    block_sector_t blocks_ptr[BLOCK_SECTOR_SIZE/4];
};

/* In-memory inode. */
struct inode {
    struct list_elem  elem;           /* Element in inode list. */
    block_sector_t    sector;         /* Sector number of disk location. */
    int               open_cnt;       /* Number of openers. */
    bool              removed;        /* True if deleted, false otherwise. */
    off_t             read_length;
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
// if you are writing pas the file then you need to allocate more blocks after - allocate blocks after 
//if offset is past hte length of the file then you would need to allocate more blcoks
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);

bool inode_reserve_disk(struct inode_disk *inode_disk, off_t length);
off_t inode_expand(struct inode_disk *inode_disk, off_t length, bool alloc);
void inode_close_helper(struct inode_disk *inode_disk);

/*
recitation notes 
directories first - go into inode_disk - bool flag for directory inode coming from directory 
giving your thread struct your working directory 
within thread execute or process execute 
filesys takes paths - use strktok_r for that not sure where 
directory - functions you need to change
you hsouldnt be able to remove a directory if it has files in it 
dir_remove - if hte name thing is an empty directory you can remove it if its not a empty then you cant remove it 
change the dirreaddir - reading all the names of the directory entries 
you also need to change dir_open 
filesys.c - all the stuff that you used in your files syscall you need to handle directories - filesys_create should handle directories 

inode needs to be changed 
offset you are finding if you are using direct singly doubly - change the bytes to sector 
inode create - allocate your direct blocks or you dont have to - 
inode close needs to be changed 
inode read at barely needs to get changed 
inode write - needs to do file growth here make new functions that allocates direct blocks indirect blocks double indirect 
everytime you need to file grow just write a bunch of zeroes to expand it 
persistence test cases - if you implemented everything then it should work - byte check wrong proboem - inode write at is wrong byte to sector might be wrong 
you are getting the wrong byte for the offset 
test case - dir bind - keeps on creating directories within directories - all the wya until you run out of disk space then makes a file then rmeoves all the files 

inode close - you need to close the directories as well 
*/
#endif /* filesys/inode.h */
