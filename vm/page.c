#include <hash.h>
#include <string.h>
#include "page.h"



//Each thread should init this, stores SPT (Supplemental Page Table)
static unsigned SPT_hash_func(const struct hash_elem *e, void *aux);
static bool SPT_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);

struct SPT* SPT_init(void) {
    struct SPT *SPT = (struct SPT*) malloc(sizeof(struct SPT));

  hash_init (&SPT->page_entries, SPT_hash_func, SPT_less_func, NULL);
  return SPT;
}

static unsigned SPT_hash_func(const struct hash_elem *e, void *aux) {
  struct SPTE *entry = hash_entry(e, struct SPTE, SPTE_hash_elem);
  return hash_int( (int)entry->upage );
}

static bool SPT_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
  const struct SPTE *a_spte = hash_entry (a, struct SPTE, SPTE_hash_elem);
  const struct SPTE *b_spte = hash_entry (b, struct SPTE, SPTE_hash_elem);

  return a_spte->upage < b_spte->upage;
}
//need hash value for page 

//create a page lookup that returns a page containing the given virtual address
struct SPTE*
lookup_page(struct SPT *suppt, void *upage){
  struct SPTE temp_spte;
  temp_spte.upage = upage;

  struct hash_elem *elem = hash_find (&suppt->page_entries, &temp_spte.SPTE_hash_elem);
  if(elem == NULL) return NULL;
  return hash_entry(elem, struct SPTE, SPTE_hash_elem);
}
bool SPTE_install_file(struct SPT *SuT, struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct SPTE *spte = (struct SPTE *) malloc(sizeof(struct SPTE));
  spte->file = file;
  spte->file_off = ofs;
  spte->upage = upage;
  spte->kpage = NULL;
  spte->page_read_bytes = read_bytes;
  spte->page_zero_bytes = zero_bytes;
  spte->writeable = writable;
  spte->page_stat = IN_FILE;

  struct hash_elem *elem_exist;
  elem_exist = hash_insert(&SuT->page_entries, &spte->SPTE_hash_elem);
  //hash_insert 
  if(elem_exist == NULL) {
    return true;
  }
  return false;
  
}

// this is the main function for loading the page for the address upage 
bool load_page(struct SPT *SuT, uint32_t *pagedir, void *upage) {
  struct SPTE *spte;
  spte = lookup_page(SuT, upage);
  if(spte == NULL) {
    return false;
  }

  if(spte->page_stat == FRAME) {
    return true;
  }

  void *frame_page = palloc_get_page (PAL_USER);
  if(frame_page == NULL) {
    return false;
  }
   bool writeable2 = true;
  // going to need to chagne this code as we get more conditions so that it reflects what we have 
  if(spte->page_stat == FRAME){
    // do nothing i think 
  }
  else if(spte->page_stat == IN_FILE) {
    size_t page_read_bytes = spte->page_read_bytes < PGSIZE ? spte->page_read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    lock_acquire(&sys_lock);
    file_seek(spte->file, spte->file_off);
    spte->kpage = frame_page;
    int read_byte = file_read(spte->file, frame_page, page_read_bytes);
    lock_release(&sys_lock);

    if(read_byte != (int)page_read_bytes) {
      //free the frame here - using frame_page 
      palloc_free_page(spte->kpage);
      //printf("this is freeing up the page\n");
      return false;
    }

    memset (spte->kpage + read_byte, 0, page_zero_bytes);

  
    writeable2 = spte->writeable;


  }
  bool status  = (pagedir_get_page(pagedir, spte->upage) == NULL);
    status = status && pagedir_set_page(pagedir, spte->upage, frame_page, true);

    if(!status) {
      //free the frame need to maek code for that -- this just means free up the frame 
      palloc_free_page(spte->kpage);
      printf("this is freeing up the page\n");
      return false;
    }

    spte->kpage = frame_page;
    spte->page_stat = FRAME;

    pagedir_set_dirty(pagedir, frame_page, false);


    return true;


}
//insert a page into supp page table 
//load page
//need something to check if a page precedes 
