#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "hash.h"

// Supplemental Page Table Entry
// Store a list of pages for each file (each file is divided into various pages spread around memory)
// Each thread should have an SPT (Hash table, which stores SPTE data structures)
struct SPTE {
    struct hash_elem SPTE_hash_elem; // Hash value for each page, lets it be part of the hash table
    // the hash is within the thread.h and each SPT is going to have a SPTE
    //the key for the hash could be the address of the page, fault address, or the page number 
    struct file *file; 
    uint8_t *kpage;
    size_t page_read_bytes;
    size_t page_zero_bytes;
    uint8_t isDirty; //Indicates of page has been modified or not
}

#endif