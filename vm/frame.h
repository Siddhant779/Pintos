#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "hash.h"
#include "page.h"
#include "bitmap.h"
#include "palloc.h"

const int FRAME_NUM = 367;
struct frame {
    void *frame_page; //pointer to the place in memory
    struct list processes; //list of processes associated with the frame
    struct SPTE *page_entry; //pointer to page table entry associated to the frame
};

struct bitmap *frame_map; //to be initialized in init

struct frame frame_table[FRAME_NUM];  //array of all frames

int evict_start; //points to the index of the last evicted frame + 1, or 0 by default

void frame_init(); //initialize frame table and frame bitmap

/*
- replace palloc_get_pages in exception.c, lazy loading, etc.
- find first empty frame table, grab the address and populate it with the necessary info (somewhat straightforward)
- update the frame table entry associated with the frame you grab
- want SPTE of the page associated in the frame you are loading in
- pass no-grow pt cases when you implement frame allocation. The grow cases are passed with stack growth
*/
struct frame *get_frame();

int evict_frame(); //eviction algorithm, should return the index of the frame corresponsing to the page that was evicted

#endif