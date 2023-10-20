#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "hash.h"
#include "page.h"

struct frame {
    struct hash_elem frame_hash_elem;
    struct list processes; //list of processes associated with the frame
    struct SPTE *page; //pointer to page table entry associated to the frame
};

struct frame *get_frame();

#endif