#include "frame.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"\


static bool install_page(void *upage, void *kpage, bool writable);
static struct lock frame_lock;

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
void *get_frame(struct SPTE *new_page, enum palloc_flags flags) {
    lock_acquire(&frame_lock);
    void *k_page;
    size_t idx = bitmap_scan_and_flip(frame_map, 0, 1, false); // returns index of first valid frame, returns BITMAP_ERROR if none
    if(idx != BITMAP_ERROR) {
        k_page = palloc_get_page(PAL_USER | flags);
        new_page->kpage = k_page;
    }
    else if(idx == BITMAP_ERROR) {
        idx = evict_frame(thread_current()->pagedir); // updating the old SPTE should be automatically handled during eviction
        bool is_dirty_page = false;
        frame_table[idx].pinned = false;
        is_dirty_page = is_dirty_page || pagedir_is_dirty(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage) || pagedir_is_dirty(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->kpage);
        if(is_dirty_page) {
            int swap_idx = putInSwapArea(frame_table[idx].page_entry->kpage);
            SPTE_set_swap(frame_table[idx].page_entry->curr->SuppT, frame_table[idx].page_entry->upage, swap_idx, frame_table[idx].page_entry->curr);
            pagedir_set_dirty(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage, true);
            pagedir_clear_page(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage);
            palloc_free_page(frame_table[idx].page_entry->kpage);
        }
        else{
            SPTE_set_in_file(frame_table[idx].page_entry->curr->SuppT, frame_table[idx].page_entry->upage, frame_table[idx].page_entry->curr);
            pagedir_clear_page(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage);
            palloc_free_page(frame_table[idx].page_entry->kpage);

        }
        k_page = palloc_get_page(PAL_USER | flags);
        new_page->kpage = k_page;
    }
    frame_table[idx].thr = thread_current();
    new_page->index_frame = idx;
    frame_table[idx].page_entry = new_page;
    lock_release(&frame_lock);
    return k_page;
}
void SPTE_set_swap(struct SPT *supT, void *upage, int swap_idx, struct thread *t) {
    struct SPTE *supTe;
    supTe = lookup_page(supT, upage);
    if(supTe == NULL) {
        return;
    }
    supTe->swap_idx = swap_idx;
    supTe->page_stat = SWAP;
    supTe->kpage == NULL;
    supTe->pinned = false;
}
void SPTE_set_in_file(struct SPT *supT, void *upage, struct thread *t) {
    struct SPTE *supTe;
    supTe = lookup_page(supT, upage);
    if(supTe == NULL) {
        return;
    }
    supTe->page_stat = IN_FILE;
    supTe->kpage == NULL;
    supTe->pinned = false;
}
void frame_init() {
    lock_init(&frame_lock);
    frame_map = bitmap_create(FRAME_NUM); // create 367 bit bitmap to keep track of frames. TODO: destroy the bitmap when done
    //you dont need to do palloc_get_page for each one - thas whats causing it to stall around 
    frame_table = (struct FTE*)malloc(sizeof(struct FTE)* FRAME_NUM);

    for(int i = 0; i < FRAME_NUM; i++) {
        frame_table[i].page_entry = NULL;
        frame_table[i].index = i;
    }
    
    evict_start = 0;

}
int evict_frame(uint32_t *pagedir) {
    int evicted = -1; // frame index of the frame you are evicting (not accessed and not dirty by default)

    struct thread *t = thread_current();
    int index = evict_start;
    while(true) {
        if(!pagedir_is_accessed(pagedir, frame_table[index].page_entry->upage) && !frame_table[index].page_entry->pinned){
            break;
        }
        pagedir_set_accessed(pagedir, frame_table[index].page_entry->upage, false);
        index = (index + 1) % FRAME_NUM;
    }
    evicted = index;

    evict_start = (evicted + 1) % FRAME_NUM;
    return evicted;
}

//Frame is the actual physical memory, pages are the info that you want to put in frames. When you are using a page, it has to be in frame, or it can be in swap area
void vm_frame_free(int index_frame) {
    
    palloc_free_page(frame_table[index_frame].page_entry->kpage);
    bitmap_reset(frame_map, index_frame);
}
static bool
install_page(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     * address, then map our page there. */
    return pagedir_get_page(t->pagedir, upage) == NULL
           && pagedir_set_page(t->pagedir, upage, kpage, writable);
}