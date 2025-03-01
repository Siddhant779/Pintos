#include <hash.h>
#include <string.h>
#include "page.h"
#include "swap.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"




//Each thread should init this, stores SPT (Supplemental Page Table)
static unsigned SPT_hash_func(const struct hash_elem *e, void *aux);
static bool SPT_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);

struct SPT* SPT_init(void) {
  lock_init(&spte_lock);
  struct SPT *SPT = (struct SPT*) malloc(sizeof(struct SPT));

  hash_init (&SPT->page_entries, SPT_hash_func, SPT_less_func, NULL);
  return SPT;
}

static unsigned SPT_hash_func(const struct hash_elem *e, void *aux) {
  struct SPTE *entry = hash_entry(e, struct SPTE, SPTE_hash_elem);
  return hash_bytes(&entry->upage, sizeof(entry->upage));
  //return hash_int((int)entry->upage);
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
  //printf("number of hash %d\n", hash_size(&suppt->page_entries));
  //printf("this is the upage %p\n", upage);
  struct hash_elem *elem = hash_find (&suppt->page_entries, &temp_spte.SPTE_hash_elem);
  if(elem == NULL) return NULL;
  return hash_entry(elem, struct SPTE, SPTE_hash_elem);
}
bool SPTE_install_file(struct SPT *SuT, struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, uint32_t *pagedir, struct thread *t)
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
  spte->pinned = false;
  spte->pagedir = pagedir;
  spte->curr = t;

  struct hash_elem *elem_exist;
  elem_exist = hash_insert(&SuT->page_entries, &spte->SPTE_hash_elem);
  //hash_insert 
  if(elem_exist == NULL) {
    return true;
  }
  return false;
  
}

bool SPTE_install_frame_setup_stack(struct SPT *SuT, uint8_t *upage, uint8_t *kpage, bool writeable)
{
  struct SPTE *spte = (struct SPTE *) malloc(sizeof(struct SPTE));
  spte->upage = upage;
  spte->kpage = kpage;
  spte->page_stat = FRAME;
  spte->writeable = writeable;

  struct hash_elem *elem_exist;
  elem_exist = hash_insert(&SuT->page_entries, &spte->SPTE_hash_elem);
  //hash_insert 
  if(elem_exist == NULL) {
    return true;
  }
  return false;
  
}

bool SPTE_install_zeropage(struct SPT *SuT, uint8_t *upage, uint32_t *pagedir, struct thread *t)
{
  struct SPTE *spte;
  spte = (struct SPTE *) malloc(sizeof(struct SPTE));

  spte->upage = upage;
  spte->kpage = NULL;
  spte->page_stat = ALL_ZERO;
  spte->pagedir = pagedir;
  spte->curr = t;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&SuT->page_entries, &spte->SPTE_hash_elem);
  if (prev_elem == NULL) return true;

  // there is already an entry -- impossible state
  PANIC("Duplicated SUPT entry for zeropage");
  return false;
}

static void destory_spte_spt(struct hash_elem *elem, void *un UNUSED) {
  struct SPTE *suppt_e = hash_entry(elem, struct SPTE, SPTE_hash_elem);
  if(suppt_e->page_stat == SWAP) {
    //printf("freeing swap\n");
    swap_free(suppt_e->swap_idx);
  }
  if(suppt_e->kpage != NULL) {
    if(suppt_e->page_stat == FRAME) {
      vm_frame_free(suppt_e->index_frame);
    }
   }
  free(suppt_e);
        
}

void 
vm_destory(struct SPT *table) {
  hash_destroy(&table->page_entries, destory_spte_spt);
}

// this is the main function for loading the page for the address upage 
bool load_page(struct SPT *SuT, uint32_t *pagedir, void *upage) {
  struct thread *t = thread_current();
  lock_acquire(&t->spt_lock);
  struct SPTE *spte;
  spte = lookup_page(SuT, upage);
  if(spte == NULL) {
    lock_release(&t->spt_lock);
    return false;
  }

  if(spte->page_stat == FRAME) {
    lock_release(&t->spt_lock);
    return true;
  }
  void *frame_page = get_frame (spte, PAL_USER);
  spte = lookup_page(SuT, upage);
  if(spte == NULL) {
    lock_release(&t->spt_lock);
    return false;
  }

  if(spte->page_stat == FRAME) {
    lock_release(&t->spt_lock);
    return true;
  }
  if(frame_page == NULL) {
    lock_release(&t->spt_lock);
    return false;
  }
   bool writeable2 = true;
  if(spte->page_stat == FRAME){
  }
  else if(spte->page_stat == SWAP) {
    getFromSwapArea(spte->swap_idx, frame_page);
    install_page(spte->upage,spte->kpage, spte->writeable);
    pagedir_set_dirty(pagedir, spte->upage, true);
    spte->page_stat = FRAME;
    spte->kpage = frame_page;
    lock_release(&t->spt_lock);
    return true;
    
  }
  else if(spte->page_stat == ALL_ZERO) {
    memset(frame_page, 0, PGSIZE);
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
      palloc_free_page(spte->kpage);
      lock_release(&t->spt_lock);
      return false;
    }

    memset (spte->kpage + read_byte, 0, page_zero_bytes);

  
    writeable2 = spte->writeable;


  }
  bool status  = (pagedir_get_page(pagedir, spte->upage) == NULL);
    status = status && pagedir_set_page(pagedir, spte->upage, frame_page, writeable2);

    if(!status) {
      //free the frame need to maek code for that -- this just means free up the frame 
      palloc_free_page(spte->kpage);
      lock_release(&t->spt_lock);
      return false;
    }

    spte->kpage = frame_page;
    spte->page_stat = FRAME;

    pagedir_set_dirty(pagedir, frame_page, false);

    lock_release(&t->spt_lock);
    return true;


}
//insert a page into supp page table 
//load page
//need something to check if a page precedes 
