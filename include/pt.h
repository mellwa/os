#ifndef _PT_H_
#define _PT_H_

#include <types.h>
#include <array.h>
#include "opt-A3.h"



#if OPT_A3

#define DIRTY 0x1
#define MODIFIED 0x2
#define VALID 0x4
#define IN_SWAP 0x8

#define TEXT 10
#define DATA 11
#define STACK 12

/* page fault types */
#define COPY_FROM_SWAP 1
#define COPY_FROM_ELF  2
#define COPY_TO_STACK  3

struct pt_entry {
    int cm_index;
    vaddr_t page_number;
    char flag;
    int swapfile_index;
};

struct pt_entry* build_ptentry(int cm_index, vaddr_t page_number, char flag);
void change_pte(struct pt_entry* pte, int cm_index, char flag);
int segment_type(vaddr_t page_number);
/* Page Fault handling function called by */
int page_fault(int faulttype, vaddr_t faultaddress);
int get_pt_entry(struct pt_entry** pte, vaddr_t vaddr);
int load_page(paddr_t paddr, vaddr_t vaddr);
int load_page_from_elf(vaddr_t vaddr);
#endif /* OPT_A3 */
#endif /* _PT_H_ */
