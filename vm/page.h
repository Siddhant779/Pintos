#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "hash.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

enum page_status {
    FRAME, // in the memory 
    SWAP, // on the swap table  
    IN_FILE, //page is in the file needs to be brought in - on disk
    ALL_ZERO // means its all zero 
    //am i missing something else
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
    off_t file_off;
    uint32_t page_read_bytes;
    uint32_t page_zero_bytes;

    void *kpage; // frame associated with this page  - could be null if there is no frame 
    void *upage; // for hte virutal address of hte page this exists as the key 

    //bool isDirty; //Indicates of page has been modified or not -- in pagedir there is a function called isDirty that checks if the SPTE is dirty
    bool writeable;

    enum page_status page_stat;

    
    // we need something for the swap here 
};


struct SPT* SPT_init(void);
bool SPTE_install_file(struct SPT *SuT, struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool load_page(struct SPT *SuT, uint32_t *pagedir, void *upage);
#endif