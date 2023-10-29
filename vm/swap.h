#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "hash.h"
#include "page.h"
#include "bitmap.h"
#include "threads/palloc.h"
#include "devices/block.h"


void swap_init(void);
void swap_insert(int swap_idx, void *k_page);
int swap_get(void *k_page);
void swap_free(int swap_idx);


#endif