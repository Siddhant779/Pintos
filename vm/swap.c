#include "swap.h"


static struct bitmap *block_tracker;
static struct block *swap_slot;

static const size_t SWAP_SLOT_SIZE = PGSIZE / BLOCK_SECTOR_SIZE;

static struct lock block_lock;

static size_t swap_size;


void swap_init(void) {
    swap_slot = block_get_role(BLOCK_SWAP);
    swap_size = block_size(swap_slot) / SWAP_SLOT_SIZE;
    block_tracker = bitmap_create(swap_size);

    bitmap_set_all(block_tracker, true);
}

int swap_get(void *k_page) {
    size_t swap_avail = bitmap_scan(block_tracker, 0, 1, true);

    for(int i = 0; i < SWAP_SLOT_SIZE; i++) {
        block_write(swap_slot, swap_avail * SWAP_SLOT_SIZE + i, k_page + (BLOCK_SECTOR_SIZE * i));

    }
    bitmap_reset(block_tracker, swap_avail);

    return swap_avail;
}

void swap_insert(int swap_idx, void *k_page) {
    ASSERT(swap_idx < swap_size);

    if(bitmap_test(block_tracker, swap_idx) == false) {
       for(int i = 0; i < SWAP_SLOT_SIZE; i++) {
            block_read(swap_slot, swap_idx * SWAP_SLOT_SIZE + i, k_page + (BLOCK_SECTOR_SIZE * i));
        }
    }
    bitmap_mark(block_tracker,swap_idx);
}

void swap_free(int swap_idx) {
    bitmap_set(block_tracker, swap_idx, true);
}