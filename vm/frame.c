#include "frame.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"\


static bool install_page(void *upage, void *kpage, bool writable);
static struct lock frame_lock;

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
        frame_table[idx].kpage = k_page;
        new_page->kpage = frame_table[idx].kpage;
    }
    else if(idx == BITMAP_ERROR) {
        idx = evict_frame(thread_current()->pagedir); // updating the old SPTE should be automatically handled during eviction
        hi_test++;
        idx = hi_test %367;
        if(idx == 0 || 4 || 6) {
            idx++;
        }
        printf("this is the idx %d\n", idx);
        pagedir_clear_page(frame_table[idx].thr->pagedir, frame_table[idx].page_entry->upage);
        bool is_dirty_page = false;
        frame_table[idx].pinned = false;
        is_dirty_page = is_dirty_page || pagedir_is_dirty(frame_table[idx].thr->pagedir, frame_table[idx].page_entry->upage) || pagedir_is_dirty(frame_table[idx].thr->pagedir, frame_table[idx].page_entry->kpage);
        if(is_dirty_page) {
            //put it in to swapArea
            int swap_idx = putInSwapArea(frame_table[idx].kpage);
            SPTE_set_swap(thread_current()->SuppT, frame_table[idx].upage, swap_idx);
            pagedir_set_dirty(frame_table[idx].thr->pagedir, frame_table[idx].page_entry->upage, true);
            frame_table[idx].page_entry->swap_idx = swap_idx;
            freeUpFrame(frame_table[idx].kpage);
            palloc_free_page(frame_table[idx].kpage);
        }
        else{
            freeUpFrame(frame_table[idx].kpage);
            palloc_free_page(frame_table[idx].kpage);

        }
        //need to set the SPTE to dirty and swap now ;
        // you need some way for the SPTE to know that this is the swap index 
        //remove from the hash here -we need the hash for pinning 
        
        // you arent putting k_page to naything here this part is wrong 
        k_page = palloc_get_page(PAL_USER | flags);
        frame_table[idx].kpage = k_page;
        new_page->kpage = frame_table[idx].kpage;
        //within the evict frame or here you actually remove the page - need to malloc for each frame then
    }
    struct FTE_hash *f = malloc(sizeof(struct FTE_hash)); // frame you wish to store the new page in
    //frame_table[idx] = *f;
    //new_page->kpage = k_page;
    frame_table[idx].upage = new_page->upage;
    frame_table[idx].thr = thread_current();
    frame_table[idx].page_entry = new_page;
    //frame_table[idx].pinned = true;
    f->index = idx;
    f->kpage = k_page;
    hash_insert(&frame_entries, &(f->frame_elem));


    //install_page(new_page->upage, f->frame, new_page->writeable); //install new page
     //update new SPT
    lock_release(&frame_lock);
    return new_page->kpage;
    //return k_page;
}
void freeUpFrame(void *kpage) {
    // this is going to be used for freeing up frames that are not dirty
    struct FTE_hash *fte_temp;
    fte_temp->kpage = kpage;
    struct hash_elem *elem = hash_find(&frame_entries,&(fte_temp->frame_elem));
    if(elem == NULL) {
        printf("there is somethign wrong with program look into this ");
    }
    struct FTE_hash *fte_pin;
    fte_pin = hash_entry(elem, struct FTE_hash, frame_elem);
    hash_delete (&(frame_entries), &(fte_pin->frame_elem));
}
void SPTE_set_swap(struct SPT *supT, void *upage, int swap_idx) {
    //lock_acquire(&frame_lock);
    struct SPTE *supTe;
    supTe = lookup_page(supT, upage);
    if(supTe == NULL) {
        return;
    }
    supTe->swap_idx = swap_idx;
    supTe->page_stat = SWAP;
    supTe->kpage == NULL;
    //lock_release(&frame_lock);
}
void frame_init() {
    lock_init(&frame_lock);
    frame_map = bitmap_create(FRAME_NUM); // create 367 bit bitmap to keep track of frames. TODO: destroy the bitmap when done
    //you dont need to do palloc_get_page for each one - thas whats causing it to stall around 
    frame_table = (struct FTE*)malloc(sizeof(struct FTE)* FRAME_NUM);

    for(int i = 0; i < FRAME_NUM; i++) {
        //frame_table[i].frame = palloc_get_page(PAL_USER); // associate a user memory page to the frame
        //list_init(&(frame_table[i].processes)); 
        frame_table[i].page_entry = NULL;
        frame_table[i].index = i;
    }
    
    hash_init(&frame_entries, FTE_hash_func, FTE_less_func, NULL);
    list_init(&frame_list);
    evict_start = 0;
    hi_test = 1;

}
void frame_by_upage(struct SPT *supT, void *upage, bool pinning) {
    struct SPTE *supTe;
    supTe = lookup_page(supT, upage);
    if(supTe == NULL) {
        return;
    }
    void *kpage = supTe->kpage;
    if(supTe->page_stat == FRAME) {
        frame_pinning(kpage, pinning);
    }
}
void frame_pinning(void *kpage, bool pin) {
    lock_acquire(&frame_lock);
    struct FTE_hash *fte_temp;
    fte_temp->kpage = kpage;
    struct hash_elem *elem = hash_find(&frame_entries,&(fte_temp->frame_elem));
    if(elem == NULL) {
        printf("there is somethign wrong with program look into this ");
    }
    struct FTE_hash *fte_pin;
    fte_pin = hash_entry(elem, struct FTE_hash, frame_elem);
    int index = fte_pin->index;
    frame_table[index]. pinned = pin;
    lock_release(&frame_lock);
}
int evict_frame(uint32_t *pagedir) {
    int i = evict_start;
    int evicted = -1; // frame index of the frame you are evicting (not accessed and not dirty by default)
    int clean = -1; // frame index of a not dirty page

    //loop though page table in a circular fashion
    // do {
    //     // honestly not sure how the pagedir functions work, this is just how I think they work
    //     //if(frame_table[i].pinned == false) {
    //         if(!pagedir_is_dirty(frame_table[i].kpage, frame_table[i].page_entry->kpage)) {
    //             if(!pagedir_is_accessed(frame_table[i].kpage, frame_table[i].page_entry->upage)) {
    //                 evicted = i; // if page was neither accessed nor dirty, evict it
    //                 break;
    //             } else {
    //                 pagedir_set_accessed(frame_table[i].kpage, frame_table[i].page_entry->upage, false);
    //                 clean = i; // if page is not dirty then consider it for eviction
    //             }
    //         }
    //     //}
    //     i = (i + 1) % FRAME_NUM;
    // } while(i != evict_start);
    // if(evicted == -1){
    //     do {
    //     // honestly not sure how the pagedir functions work, this is just how I think they work
    //     //if(frame_table[i].pinned == false) {
    //         if(!pagedir_is_dirty(frame_table[i].kpage, frame_table[i].page_entry->kpage)) {
    //             if(!pagedir_is_accessed(frame_table[i].kpage, frame_table[i].page_entry->upage)) {
    //                 evicted = i; // if page was neither accessed nor dirty, evict it
    //                 break;
    //             } else {
    //                 clean = i; // if page is not dirty then consider it for eviction
    //             }
    //         }
    //         i = (i + 1) % FRAME_NUM;
    //     } while(i != evict_start);
    // }
    for(i = evict_start; i < FRAME_NUM * 2; i++) {
        if(i !=0 || i !=367){
            if(pagedir_is_accessed(pagedir, frame_table[i].upage)){
                pagedir_set_accessed(pagedir, frame_table[i].upage, false);
            }
            else {
                evicted = i;
                break;
            }
        }
    }
    void* newArea = NULL;
    
    // if no frame was neither accessed nor dirty, evict a non-dirty frame
    if(evicted == -1) {
        evicted = clean;
        //if all frames are dirty, evict a random frame and swap the page to SWAP SPACE
        if(evicted == -1) {
            evicted = 40;
            //evicted = 30; //evict some random frame
            //newArea = putInSwapArea(frame_table[evicted].frame); //place old frame data into swap space
        }
    }
    
    // may need to write frame data to disk but not sure how to do this
    evicted = evicted %FRAME_NUM;
    // remove association between frame and evicted SPTE
    struct SPTE *old_page = frame_table[evicted].page_entry; // SPTE of old page associated to the frame
    old_page->kpage = newArea; 

    evict_start = (evicted + 1) % FRAME_NUM; //update evict_start
    return evicted; //return the frame index of the page you evicted
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