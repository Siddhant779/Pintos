#include <debug.h>
#include <round.h>
#include <string.h>

#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define SECTOR_SIZE 512

#define MAX_FILE_SIZE 8980480


/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
 * within INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static block_sector_t
byte_to_sector(const struct inode *inode, off_t size, off_t pos)
{
    ASSERT(inode != NULL);
    // if (0 <= pos && pos < inode->data.length) {
    //     size_t target_sectors = bytes_to_sectors(pos);
    //     //return inode->data.start + pos / BLOCK_SECTOR_SIZE;
    // } else {
    //     return -1;
    // }
    size_t pos_sector = bytes_to_sectors(pos);
    if(0 <= pos && pos < size) {
        //printf("this is the pos %d", pos);
        uint32_t index_sector;
        uint32_t blocks[128]; // this is for indirect pointer this is where you would store from block_reads as seen below 
        for(int i = 0; i< 128; i++) {
            blocks[i] = 0;
        }
        if(pos < SECTOR_SIZE * DIRECT_BLOCKS) {
            //printf("direct inode\n");
            return inode->data.block_array[pos/SECTOR_SIZE];
        }
        else if (pos < SECTOR_SIZE *(DIRECT_BLOCKS + 128)) { // equation i got from OH
            pos -= SECTOR_SIZE*DIRECT_BLOCKS;
            index_sector = pos/(SECTOR_SIZE * 128) + DIRECT_BLOCKS;
            block_read(fs_device, inode->data.block_array[index_sector], &blocks);
            pos %= BLOCK_SECTOR_SIZE*128;
            return blocks[pos/BLOCK_SECTOR_SIZE];
        }
        else {
            block_read(fs_device, inode->data.block_array[DOUBLE_INDIRECT_INDEX], &blocks);
            pos -= SECTOR_SIZE*(DIRECT_BLOCKS + 128);
            index_sector = pos/(BLOCK_SECTOR_SIZE * 128);
            block_read(fs_device, blocks[index_sector], &blocks);
            pos %= BLOCK_SECTOR_SIZE * 128;
            return blocks[pos/BLOCK_SECTOR_SIZE];
        }
    }
    else {
        return -1;
    }
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init(void)
{
    list_init(&open_inodes);
}

off_t inode_expand(struct inode_disk *inode_disk, off_t length, bool alloc) {
    static char zero_space[BLOCK_SECTOR_SIZE];
    for(int r =0; r< BLOCK_SECTOR_SIZE; r++) {
        zero_space[r] = 0;
    }
    struct contents content_block;
    struct contents content_block_double;
    struct contents content_block_double_inner;
    for(int i = 0; i < 128; i++) {
        content_block.blocks_ptr[i] = 0;
        content_block_double.blocks_ptr[i] = 0;
        content_block_double_inner.blocks_ptr[i] = 0;
    }
    //printf("this is the inode %p and disk index %d", inode_disk, inode_disk->direct_index);
    size_t old_sectors = bytes_to_sectors(inode_disk->length);
    size_t new_sectors = bytes_to_sectors(length);
    if(alloc) {
        old_sectors = 0;
    }
    new_sectors = new_sectors - old_sectors;
    if(new_sectors == 0) {
        return length;
    }

    while(inode_disk->direct_index < INDIRECT_INDEX) {
        free_map_allocate(1, &inode_disk->block_array[inode_disk->direct_index]);
        block_write(fs_device, inode_disk->block_array[inode_disk->direct_index], zero_space); // putting zeroes for expanding it 
        inode_disk->direct_index++;
        new_sectors--;
        if(new_sectors == 0) {
            return length;
        }
    }
    if(inode_disk->direct_index == INDIRECT_INDEX) {
        if(inode_disk->indirect_index == 0) {
            free_map_allocate(1, &inode_disk->block_array[inode_disk->direct_index]);
        }
        else {
            block_read(fs_device, inode_disk->block_array[inode_disk->direct_index], &content_block);
        }
        while(inode_disk->indirect_index < 128) {
            free_map_allocate(1, &content_block.blocks_ptr[inode_disk->indirect_index]);
            block_write(fs_device, content_block.blocks_ptr[inode_disk->indirect_index], zero_space);
            inode_disk->indirect_index++;
            new_sectors--;
            if(new_sectors == 0) {
                break;
            }
        }
        block_write(fs_device, inode_disk->block_array[inode_disk->direct_index], &content_block);
        if(inode_disk->indirect_index == 128) {
            inode_disk->indirect_index = 0;
            inode_disk->direct_index++;
        }
        if(new_sectors == 0){
            return length;
        }
    }
    if(inode_disk->direct_index == DOUBLE_INDIRECT_INDEX){
        if(inode_disk->double_indirect_index == 0 && inode_disk->indirect_index == 0){
            free_map_allocate(1, &inode_disk->block_array[inode_disk->direct_index]);
        }
        else {
            block_read(fs_device, inode_disk->block_array[inode_disk->direct_index], &content_block_double);
        }
        while(inode_disk->indirect_index < 128) {
            if(inode_disk->double_indirect_index == 0) {
                free_map_allocate(1, &content_block_double.blocks_ptr[inode_disk->indirect_index]);
            }
            else{
                block_read(fs_device, content_block_double.blocks_ptr[inode_disk->indirect_index], &content_block_double_inner);
            }
            while(inode_disk->double_indirect_index < 128) {
                free_map_allocate(1, &content_block_double_inner.blocks_ptr[inode_disk->double_indirect_index]);
                block_write(fs_device, content_block_double_inner.blocks_ptr[inode_disk->double_indirect_index], zero_space);
                inode_disk->double_indirect_index++;
                new_sectors--;
                if(new_sectors == 0) {
                    break;
                }
            }
            block_write(fs_device, content_block_double.blocks_ptr[inode_disk->indirect_index], &content_block_double_inner);
            if(inode_disk->double_indirect_index == 128) {
                inode_disk->double_indirect_index = 0;
                inode_disk->indirect_index++;

            }
            if(new_sectors == 0){
                break;
            }
        }
        block_write(fs_device, inode_disk->block_array[inode_disk->direct_index], &content_block_double);
        return length;
    }
}
void inode_close_helper_double(block_sector_t *ptr_block, size_t data_sub){
    struct contents content_block;
     for(int i = 0; i < 128; i++) {
        content_block.blocks_ptr[i] = 0;
    }
    block_read(fs_device, *ptr_block, &content_block);
    for (int i = 0; i < data_sub; i++)
        {
        free_map_release(content_block.blocks_ptr[i], 1);
        }
    free_map_release(*ptr_block, 1);
}
void inode_close_helper(struct inode_disk *inode_disk) {
    size_t direct_block_sectors = bytes_to_sectors(inode_disk->length);
    size_t indirect_block_sectors = 0;
    off_t size = inode_disk->length;
    if(size - (BLOCK_SECTOR_SIZE * DIRECT_BLOCKS) >= 0) {
        size -= BLOCK_SECTOR_SIZE * DIRECT_BLOCKS;
        indirect_block_sectors = DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE*128);
    }
    size_t double_indirect_block_sectors = 0;
    off_t size2 = inode_disk->length;
    if(size2 - (BLOCK_SECTOR_SIZE *(DIRECT_BLOCKS + 128)) >= 0) {
        double_indirect_block_sectors = 1;
    }
    int i = 0;
    // for(i = 0; i < INDIRECT_INDEX; i++) {
    //     if(!(direct_block_sectors > 0)){
    //         break;
    //     }
    //     direct_block_sectors--;
    //     free_map_release(inode_disk->block_array[i],1);
    // }
    while(i < INDIRECT_INDEX && direct_block_sectors > 0){
        // if(!(direct_block_sectors > 0)){
        //     break;
        // }
        free_map_release(inode_disk->block_array[i],1);
        direct_block_sectors--;
        i++;
    }
    // for(;i < DOUBLE_INDIRECT_INDEX; i++) {
    //     if(!(indirect_block_sectors > 0)){
    //         break;
    //     }
    //     indirect_block_sectors--;
    //     free_map_release(inode_disk->block_array[i],1);
    // }
     while (indirect_block_sectors > 0 && i < DOUBLE_INDIRECT_INDEX)
    {
      size_t data_sub = direct_block_sectors < 128 ? \
	    direct_block_sectors : 128;
      inode_close_helper_double(&inode_disk->block_array[i],data_sub);
      direct_block_sectors -= data_sub;
      indirect_block_sectors--;
      i++;
    }
    size_t direct_block_sectors_temp = direct_block_sectors;
    while(double_indirect_block_sectors > 0) {
        struct contents content_block;
         for(int s = 0; s < 128; s++) {
            content_block.blocks_ptr[s] = 0;
        }
        block_read(fs_device, inode_disk->block_array[i], &content_block);
        for (int r = 0; i < indirect_block_sectors; r++)
        {
            size_t data_sub = direct_block_sectors < 128 ? direct_block_sectors : \
	            128;
            inode_close_helper_double(&content_block.blocks_ptr[r], data_sub);
            direct_block_sectors_temp -= data_sub;
        }
        free_map_release(inode_disk->block_array[i], 1);
        double_indirect_block_sectors--;
    }
}
/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * device.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
bool
inode_reserve_disk(struct inode_disk *inode_disk, off_t length){
    if(length < 0) {
        return false;
    }
    inode_expand(inode_disk, length, true);
    return true;
    //the math with the inode disk is length- (120*512)
    //if its less than that then that means that you can use that one - have to find the offset for that tho 

}
bool
inode_create(block_sector_t sector, off_t length, bool dir)
{
    struct inode_disk *disk_inode = NULL;
    bool success = false;
    //printf("goes through inode create \n");
    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
     * one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {
        size_t sectors = bytes_to_sectors(length);
        disk_inode->length = length;
        /*
        DO YOU NEED TO DO ANYTHING FOR THE MAX FILE SIZE HERE 
        
        */
        disk_inode->magic = INODE_MAGIC;
        //Modify to take data if it is directory or not directory (dir is given when this is called from syscall create)
        disk_inode->directory = dir;
        disk_inode->start = ROOT_DIR_INDEX;
        if(inode_reserve_disk(disk_inode, disk_inode->length)){
            block_write(fs_device, sector, disk_inode);
            success = true;
        }
        free(disk_inode);

        //TODO: This is a draft of how spaces for contents for entries of a directory will be allocated, change later when inode_disk is changed
        // if(!dir){disk_inode->dirContents == NULL;}
        // else{disk_inode->dirContents = calloc(1, sizeof(struct contents*));} //Allocate a sector for dir contents (idk if corrent)

        //if (free_map_allocate(sectors, &disk_inode->start)) {
        //     block_write(fs_device, sector, disk_inode);
        //     if (sectors > 0) {
        //         static char zeros[BLOCK_SECTOR_SIZE];
        //         size_t i;

        //         for (i = 0; i < sectors; i++) {
        //             block_write(fs_device, disk_inode->start + i, zeros);
        //         }
        //     }
        //     success = true;
        // }
        // free(disk_inode);

    }
    return success;
}

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
    struct list_elem *e;
    struct inode *inode;
    //printf("goes through inode open \n");
    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            return inode;
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL) {
        return NULL;
    }

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    block_read(fs_device, inode->sector, &inode->data);
    inode->read_length = inode->data.length;
    return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
    if (inode != NULL) {
        inode->open_cnt++;
    }
    return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
    return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close(struct inode *inode)
{
    /* Ignore null pointer. */
    if (inode == NULL) {
        return;
    }

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        //printf("goes through inode close\n");
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);

        /* Deallocate blocks if removed. */
        if (inode->removed) {
            free_map_release(inode->sector, 1);
            inode_close_helper(&inode->data);
            //free_map_release(inode->data.start,
                             //bytes_to_sectors(inode->data.length));
        }

        free(inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove(struct inode *inode)
{
    ASSERT(inode != NULL);
    inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    uint8_t *bounce = NULL;

    off_t length = inode->read_length;
    
    if(offset >= length) {
        return 0;
    }

    while (size > 0) {
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, length, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = length - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0) {
            break;
        }
        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
            /* Read full sector directly into caller's buffer. */
            block_read(fs_device, sector_idx, buffer + bytes_read);
        } else {
            /* Read sector into bounce buffer, then partially copy
             * into caller's buffer. */
            if (bounce == NULL) {
                bounce = malloc(BLOCK_SECTOR_SIZE);
                if (bounce == NULL) {
                    break;
                }
            }
            block_read(fs_device, sector_idx, bounce);
            memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }
    free(bounce);

    return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at(struct inode *inode, const void *buffer_, off_t size,
               off_t offset)
{
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    uint8_t *bounce = NULL;

    if (inode->deny_write_cnt) {
        return 0;
    }
    if(offset + size > inode_length(inode)) {
       // LOCK FOR THE INODE 
       off_t l = inode_expand(&inode->data, offset + size, false);
       //printf("l: %d\n", l);
       inode->data.length = l;
       block_write(fs_device, inode->sector, &inode->data);
    }

    while (size > 0) {
        /* Sector to write, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, inode_length(inode), offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0) {
            break;
        }

        if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
            /* Write full sector directly to disk. */
            block_write(fs_device, sector_idx, buffer + bytes_written);
        } else {
            /* We need a bounce buffer. */
            if (bounce == NULL) {
                bounce = malloc(BLOCK_SECTOR_SIZE);
                if (bounce == NULL) {
                    break;
                }
            }

            /* If the sector contains data before or after the chunk
             * we're writing, then we need to read in the sector
             * first.  Otherwise we start with a sector of all zeros. */
            if (sector_ofs > 0 || chunk_size < sector_left) {
                block_read(fs_device, sector_idx, bounce);
            } else {
                memset(bounce, 0, BLOCK_SECTOR_SIZE);
            }
            memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
            block_write(fs_device, sector_idx, bounce);
        }

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }
    free(bounce);
    inode->read_length = inode_length(inode);
    return bytes_written;
}

/* Disables writes to INODE.
 * May be called at most once per inode opener. */
void
inode_deny_write(struct inode *inode)
{
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write(struct inode *inode)
{
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length(const struct inode *inode)
{
    return inode->data.length;
}
