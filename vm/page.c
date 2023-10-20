#include <hash.h>
#include <string.h>
#include "page.h"
//Each thread should init this, stores SPT (Supplemental Page Table)
void SPT_init(struct hash *table){
    hash_init(); //should have 4 parameters - table, page hash, precede page 
}

struct SPT* SPT_init(void) {
struct SPT *SPT =
    (struct SPT*) malloc(sizeof(struct SPT));

  hash_init (&SPT->page_entries, spte_hash_func, spte_less_func, NULL);
  return SPT;
}
//need hash value for page 

//need something to check if a page precedes 
