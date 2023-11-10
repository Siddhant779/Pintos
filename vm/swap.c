#include "swap.h"


static struct bitmap *block_tracker;
static struct block *swap_block;

static const size_t SWAP_SLOT_SIZE = PGSIZE / BLOCK_SECTOR_SIZE;


static size_t swap_size;


void swap_init(void) {
    //Get swap block (entire swap area 4MB)
    swap_block = block_get_role(BLOCK_SWAP);
    //Get size of the whole swap area in units of sectors (Probably a very large number since the entire block device can store many sectors)
    swap_size = block_size(swap_block) / SWAP_SLOT_SIZE;
     //Create bitmap of to track of all the swap slots in the swap area (each slot has 8 sectors)
    block_tracker = bitmap_create(swap_size);
    // if its true that means its free; false means its occupied 
    bitmap_set_all(block_tracker, true);
}

int putInSwapArea(void *k_page) {
    bool has_lock = lock_held_by_current_thread(&sys_lock);
    if(has_lock) {
        lock_release(&sys_lock);
    }
    lock_acquire(&sys_lock);
     //Read all slots in swap area, find a slot in swap area to put the frae
    size_t swap_avail = bitmap_scan_and_flip(block_tracker, 0, 1, true);
     //Write to 8 sectors. This takes whatever data from frame and put it on the swap area
    for(int i = 0; i < SWAP_SLOT_SIZE; i++) {
        //Write data 
        block_write(swap_block, swap_avail * SWAP_SLOT_SIZE + i, k_page + (BLOCK_SECTOR_SIZE * i));

    }
    lock_release(&sys_lock);
    if(has_lock) {
        lock_acquire(&sys_lock);
    }

    return swap_avail;
}

//Function for swapping a frame with something on swap table. When you put a page into swap area, SPTE should store the index of it so you can get it back later
void getFromSwapArea(int swap_idx, void *k_page) {
    bool has_lock = lock_held_by_current_thread(&sys_lock);
    if(has_lock) {
        lock_release(&sys_lock);
    }
    lock_acquire(&sys_lock);
    ASSERT(swap_idx < swap_size);

    if(bitmap_test(block_tracker, swap_idx) == false) {
        //Read from 8 sectors so the frame can be obtained from swap area
       for(int i = 0; i < SWAP_SLOT_SIZE; i++) {
            //Read data (not sure what the purpose of each parameter here is)
            block_read(swap_block, swap_idx * SWAP_SLOT_SIZE + i, k_page + (BLOCK_SECTOR_SIZE * i));
        }
    }
    bitmap_mark(block_tracker,swap_idx);
    lock_release(&sys_lock);
    if(has_lock) {
        lock_acquire(&sys_lock);
    }

}

void swap_free(int swap_idx) {
    bool has_lock = lock_held_by_current_thread(&sys_lock);
    if(has_lock) {
        lock_release(&sys_lock);
    }
    lock_acquire(&sys_lock);
    bitmap_set(block_tracker, swap_idx, true);
    lock_release(&sys_lock);
    if(has_lock) {
        lock_acquire(&sys_lock);
    }
}