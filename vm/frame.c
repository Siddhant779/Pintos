#include "frame.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"\


static bool install_page(void *upage, void *kpage, bool writable);
static struct lock frame_lock;

static unsigned clock_ptr, clock_max;

static unsigned FTE_hash_func(const struct hash_elem *e, void *aux);
static bool FTE_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);


static unsigned FTE_hash_func(const struct hash_elem *e, void *aux) {
  struct FTE_hash *entry = hash_entry(e, struct FTE_hash, frame_elem);
  return hash_int( (int)entry->kpage );
}

static bool FTE_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
  struct FTE_hash *a_entry = hash_entry(a, struct FTE_hash, frame_elem);
  struct FTE_hash *b_entry = hash_entry(b, struct FTE_hash, frame_elem);

  return a_entry->kpage < b_entry->kpage;
}

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
        // printf("FRAME: evicting frame at idx %d with upage %p and kpage %p\n", idx, frame_table[idx].page_entry->upage, frame_table[idx].page_entry->kpage);
        bool is_dirty_page = false;
        frame_table[idx].pinned = false;
        is_dirty_page = is_dirty_page || pagedir_is_dirty(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage) || pagedir_is_dirty(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->kpage);
        if(is_dirty_page) {
            int swap_idx = putInSwapArea(frame_table[idx].page_entry->kpage);
            SPTE_set_swap(frame_table[idx].page_entry->curr->SuppT, frame_table[idx].page_entry->upage, swap_idx);
            pagedir_set_dirty(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage, true);
            pagedir_clear_page(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage);
            palloc_free_page(frame_table[idx].page_entry->kpage);
        }
        else{
            SPTE_set_in_file(frame_table[idx].page_entry->curr->SuppT, frame_table[idx].page_entry->upage);
            pagedir_clear_page(frame_table[idx].page_entry->pagedir, frame_table[idx].page_entry->upage);
            palloc_free_page(frame_table[idx].page_entry->kpage);

        }
        k_page = palloc_get_page(PAL_USER | flags);
        new_page->kpage = k_page;
    }
    struct FTE_hash *f = malloc(sizeof(struct FTE_hash)); // frame you wish to store the new page in
    frame_table[idx].thr = thread_current();
    frame_table[idx].page_entry = new_page;
    f->index = idx;
    f->kpage = k_page;
    hash_insert(&frame_entries, &(f->frame_elem));
    lock_release(&frame_lock);
    return k_page;
}
void SPTE_set_swap(struct SPT *supT, void *upage, int swap_idx) {
    // struct thread *t = thread_current();
    // bool has_sys_lock = lock_held_by_current_thread(&t->spt_lock);
    // if(has_sys_lock){
    //     lock_release(&t->spt_lock);
    // }
    // lock_acquire(&t->spt_lock);
    struct SPTE *supTe;
    supTe = lookup_page(supT, upage);
    if(supTe == NULL) {
        return;
    }
    supTe->swap_idx = swap_idx;
    supTe->page_stat = SWAP;
    supTe->kpage == NULL;
    supTe->pinned = false;
    // lock_release(&t->spt_lock);
    // if(has_sys_lock){
    //     lock_acquire(&t->spt_lock);
    // }
}
void SPTE_set_in_file(struct SPT *supT, void *upage) {
    struct thread *t = thread_current();
    // bool has_sys_lock = lock_held_by_current_thread(&t->spt_lock);
    // if(has_sys_lock){
    //     lock_release(&t->spt_lock);
    // }
    // lock_acquire(&t->spt_lock);
    struct SPTE *supTe;
    supTe = lookup_page(supT, upage);
    if(supTe == NULL) {
        return;
    }
    supTe->page_stat = IN_FILE;
    supTe->kpage == NULL;
    supTe->pinned = false;
    // lock_release(&t->spt_lock);
    // if(has_sys_lock){
    //     lock_acquire(&t->spt_lock);
    // }
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
    
    hash_init(&frame_entries, FTE_hash_func, FTE_less_func, NULL);
    list_init(&frame_list);
    evict_start = 0;
    clock_ptr = 0;
    clock_max = FRAME_NUM;
    hi_test = 1;

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

//Function for swapping a frame with something on swap table
// void* putInSwapArea(void* frame, size_t* index){
//     //Get swap block (entire swap area 4MB)
//     struct block* swapBlock = block_get_role(BLOCK_SWAP);

//     //Get size of the whole swap area in units of sectors (Probably a very large number since the entire block device can store many sectors)
//     block_sector_t blockSize = block_size(swapBlock);

//     //Create bitmap of to track of all the swap slots in the swap area (each slot has 8 sectors)
//     struct bitmap* slots = bitmap_create(blockSize/8); //num of slots (8 sectors per slot)

//     //Read all slots in swap area, find a slot in swap area to put the frae
//     size_t slotIndex = bitmap_scan_and_flip(slots, 0, 1, false); //Gives index of first free slot. It will find the first zero and flip it to one

//     //Passes information to parameter about index of frame in the swap area
//     *index = slotIndex;

//     //Write to 8 sectors. This takes whatever data from frame and put it on the swap area
//     for(int i = 0; i < 8; i++) {
//         //Get where data input (buffer + offset)
//         void* bufferRead = frame + BLOCK_SECTOR_SIZE * i;

//         //Write data 
//         block_write(swapBlock /*entire swap area*/, slotIndex * 8 + i /*sector is slot * 8 (8 sectors per slot) + i (offset)*/, bufferRead /*Information to write*/);
//     }

//     return frame;

// }

//Function for swapping a frame with something on swap table. When you put a page into swap area, SPTE should store the index of it so you can get it back later
// void* getFromSwapArea(void* frame, size_t slotIndex){
//     //Get swap block (entire swap area 4MB)
//     struct block* swapBlock = block_get_role(BLOCK_SWAP);

//     //Get size of the whole swap area in units of sectors (Probably a very large number since the entire block device can store many sectors)
//     block_sector_t blockSize = block_size(swapBlock);

//     //Read from 8 sectors so the frame can be obtained from swap area
//     for(int i = 0; i < 8; i++){
//         //Get where to read (buffer + offset)
//         void* bufferRead = frame + BLOCK_SECTOR_SIZE * i;

//         //Read data (not sure what the purpose of each parameter here is)
//         block_read(swapBlock, slotIndex * 8 + 1, bufferRead);
//     }
//     return frame;
// }

//Frame is the actual physical memory, pages are the info that you want to put in frames. When you are using a page, it has to be in frame, or it can be in swap area

static bool
install_page(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     * address, then map our page there. */
    return pagedir_get_page(t->pagedir, upage) == NULL
           && pagedir_set_page(t->pagedir, upage, kpage, writable);
}