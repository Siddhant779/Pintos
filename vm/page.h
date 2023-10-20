#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "hash.h"

enum page_status {
    FRAME, // in the memory
    SWAP, //on the swap table 
    ALL_ZERO // all zeros 
    //am i missing something else ?
}; 
// Supplemental Page Table Entry
// Store a list of pages for each file (each file is divided into various pages spread around memory)
// Each thread should have an SPT (Hash table, which stores SPTE data structures)
struct SPT {
    struct hash page_entries;
};

struct SPTE {
    struct hash_elem SPTE_hash_elem; // Hash value for each page, lets it be part of the hash table
    // the hash is within the thread.h and each SPT is going to have a SPTE
    //the key for the hash could be the address of the page, fault address, or the page number 
    struct file *file; 

    void *kpage; // frame associated with this page  - could be null if there is no frame 
    void *upage; // for hte virutal address of hte page this exists as the key 

    size_t page_read_bytes;
    size_t page_zero_bytes;

    bool isDirty; //Indicates of page has been modified or not
    bool writeable;

    enum page_status page_stat;
    // we need something for the swap here 
};

#endif