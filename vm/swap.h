#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "hash.h"
#include "page.h"
#include "bitmap.h"
#include "threads/palloc.h"
#include "devices/block.h"


void swap_init(void);
void getFromSwapArea(int swap_idx, void *k_page);
int putInSwapArea(void *k_page);
void swap_free(int swap_idx);


#endif