#include "frame.h"
#include "pagedir.h"

struct frame *get_frame() {
    // look through bitmap for a value = 0 and return it
    // if no such value exists, run eviction algorithm
}

void frame_init() {
    frame_map = bitmap_create(FRAME_NUM); // create 367 bit bitmap to keep track of frames. TODO: destroy the bitmap when done
    for(int i = 0; i < FRAME_NUM; i++) {
        frame_table[i].frame_page = palloc_get_page(PAL_USER); // associate a user memory page to the frame
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
        // TODO: f SPTE corresponding to frame was accessed and dirty, set evicted = i and break
        // TODO: else if dirty but not accessed, set clean = i
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
    evict_start = (evicted + 1) % FRAME_NUM; //update evict_start
    return evicted; //return the frame index of the page you evicted
}