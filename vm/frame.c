#include "frame.h"
#include "pagedir.h"
#include "userprog/process.h"

/* Frame logic (add locks here)
 * Input: pointer to SPTE that you wish to associate to a frame
 * if frame is available, allocate to process that requested (link SPTE)
 * 2. If no empty frame, evict a frame (clock algo)
 * 3. Frame allocated to process that requested a frame
 * 4. updates SPT/SPTE of process that use to own the frame
 *     - SPTE should be located in disk or swap, not memory
 * 5. install_page()
 * 6. Update SPT of process that just obtained new frame
 *     - should be in memory */
void *get_frame(struct SPTE *new_page) {
    size_t idx = bitmap_scan_and_flip(frame_map, 0, 1, false); // 1
    if(idx == BITMAP_ERROR) {
        idx = evict_frame(); // 2
    }
    
    struct FTE *f = &frame_table[idx]; // frame you wish to store the new page in

    f->page_entry = new_page;
    install_page(new_page->upage, f->frame, true);
    new_page->kpage = f->frame;
    return f->frame;
}

void frame_init() {
    frame_map = bitmap_create(FRAME_NUM); // create 367 bit bitmap to keep track of frames. TODO: destroy the bitmap when done
    for(int i = 0; i < FRAME_NUM; i++) {
        frame_table[i].frame = palloc_get_page(PAL_USER); // associate a user memory page to the frame
        list_init(&(frame_table[i].processes)); 
        frame_table[i].page_entry = NULL;
    }
    evict_start = 0;
}

int evict_frame() {
    int i = evict_start;
    int evicted = -1; // frame index of the frame you are evicting (not accessed and not dirty by default)
    int clean = -1; // frame index of a not dirty page

    //loop though page table in a circular fashion
    do {
        // honestly not sure how the pagedir functions work, this is just how I think they work
        if(!pagedir_is_dirty(frame_table[i].frame, frame_table[i].page_entry->kpage)) {
            if(!pagedir_is_accessed(frame_table[i].frame, frame_table[i].page_entry->kpage)) {
                evicted = i; // if page was neither accessed nor dirty, evict it
                break;
            } else {
                clean = i; // if page is not dirty then consider it for eviction
            }
        }
        i = (i + 1) % FRAME_NUM;
    } while(i != evict_start);
    
    // if no frame was neither accessed nor dirty, evict a non-dirty frame
    if(evicted == -1) {
        evicted = clean;
        //if all frames are dirty, evict a random frame and swap the page to SWAP SPACE
        if(evicted == -1) {
            evicted = 0; //set super_clean to some random page number
            // TODO: SWAP super_clean to SWAP SPACE
        }
    }
    
    // remove association between frame and evicted SPTE
    struct SPTE *old_page = frame_table[evicted].page_entry; // SPTE of old page associated to the frame
    old_page->kpage = NULL; 

    evict_start = (evicted + 1) % FRAME_NUM; //update evict_start
    return evicted; //return the frame index of the page you evicted
}

//Function for swapping a frame with something on swap table
void* page_swap(void* frame){
    //Get swap block
    struct block* swapBlock = block_get_role(BLOCK_SWAP);

    //Get size of the whole swap area in units of sectors (Probably a very large number since the entire block device can store many sectors)
    block_sector_t blockSize = block_size(swapBlock);

    //Create bitmap of to track of all the swap slots in the swap area (each slot has 8 sectors)
    struct bitmap* slots = bitmap_create(blockSize/8);

    //Read all slots in swap area
    size_t slotIndex = bitmap_scan_and_flip(slots, 0, 1, false); //Gives index of first free slot (hasn't been read from yet)
    void* bufferSlot = (void*)malloc(BLOCK_SECTOR_SIZE * 8); //Buffer to store information for 8 sectors (1 slot)

    //Read from 8 sectors
    for(for int i = 0; i < 8; i++){
        //Get where to read (buffer + offset)
        void* bufferRead = bufferSlot + BLOCK_SECTOR_SIZE * i;

        //Read data (not sure what the purpose of each parameter here is)
        block_read(swapBlock, blockSize, bufferRead);
    }

    //Write to 8 sectors
    for(for int i = 0; i < 8; i++){
        //Get where to read (buffer + offset)
        void* bufferRead = bufferSlot + BLOCK_SECTOR_SIZE * i;

        //Write data
        block_write(swapBlock, blockSize, bufferRead);
    }

}