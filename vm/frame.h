#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "hash.h"
#include "page.h"
#include "bitmap.h"
#include "threads/palloc.h"
#include "devices/block.h"
#include "list.h"
#include "vm/swap.h"

#define FRAME_NUM 367

struct FTE {
    uint32_t index;
    void *kpage; //pointer to the actual "frame" of data itself
    struct list_elem processes; //list of processes associated with the frame
    struct SPTE *page_entry; //pointer to page table entry associated to the frame
    struct thread *thr;

    bool pinned; // this is something we need for the eviction alogorithm - if a pin is framed that means we shouldn't remove it 
};

struct list frame_list; // this is the main frame list 

struct bitmap *frame_map; //to be initialized in init

//struct FTE frame_table[FRAME_NUM];  //array of all frames

struct FTE *frame_table;

int evict_start; //points to the index of the last evicted frame + 1, or 0 by default

void frame_init(void); //initialize frame table and frame bitmap

/*
- replace palloc_get_pages in exception.c, lazy loading, etc.
- find first empty frame table, grab the address and populate it with the necessary info (somewhat straightforward)
- update the frame table entry associated with the frame you grab
- want SPTE of the page associated in the frame you are loading in
- pass no-grow pt cases when you implement frame allocation. The grow cases are passed with stack growth
*/
void *get_frame(struct SPTE *new_page, enum palloc_flags flags);

int evict_frame(uint32_t *pagedir); //eviction algorithm, should return the index of the frame corresponsing to the page that was evicted


#endif