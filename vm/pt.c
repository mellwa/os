#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <coremap.h>
#include <vm.h>
#include <synch.h>
#include <mips/tlb.h>
#include <mainbus.h>
#include <pt.h>
#include <swapfile.h>
#include <current.h>
#include <addrspace.h>
#include <coremap.h>
#include <uio.h>
#include <kern/iovec.h>
#include <uw-vmstats.h>
#include "opt-A3.h"

#if OPT_A3

struct pt_entry* build_ptentry(int cm_index,vaddr_t page_number, char flag){
    struct pt_entry* pte = kmalloc(sizeof(struct pt_entry));
    if(pte == NULL){
        return NULL;
    }
    pte->flag = 0;
    pte->cm_index = cm_index;
    pte->page_number = page_number;
    pte->flag |= flag;
    pte->swapfile_index = -1;
    return pte;
}

void change_pte(struct pt_entry* pte,int cm_index, char flag){
    pte->cm_index = cm_index;
    pte->flag |= flag; //still need to add something
}

int segment_type(vaddr_t page_number){
    struct addrspace* as = curproc->p_addrspace;
    if(as == NULL){
        return -1;
    }
    /* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	#ifndef OPT_A3
	KASSERT(as->as_pbase1 != 0);
	#endif
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	#ifndef OPT_A3
	KASSERT(as->as_pbase2 != 0);
	#endif
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
    
    vaddr_t vbase1,vtop1,vbase2,vtop2,stackbase,stacktop;
	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - 12 * PAGE_SIZE;
	stacktop = USERSTACK;
    
	if (page_number >= vbase1 && page_number < vtop1) {
		return TEXT;
	}
	else if (page_number >= vbase2 && page_number < vtop2) {
		return DATA;
	}
	else if (page_number >= stackbase && page_number < stacktop) {
		return STACK;
	}
	else {
		return -1; //EFAULT
	}
    
}

int get_pt_entry(struct pt_entry** pte, vaddr_t vaddr){
    struct addrspace* as = curproc->p_addrspace;
    int seg_type;
    int index;
    seg_type = segment_type(vaddr);
    if(seg_type == TEXT){
        //remember to check vbase1 is PAGE_FRAME aligned
        index = (vaddr&PAGE_FRAME)/PAGE_SIZE - as->as_vbase1/PAGE_SIZE;
        *pte = array_get(as->as_textSeg,index);
    }
    else if(seg_type == DATA){
        index = (vaddr&PAGE_FRAME)/PAGE_SIZE - as->as_vbase2/PAGE_SIZE;
        *pte = array_get(as->as_dataSeg,index);
    }
    else if(seg_type == STACK){
        index = 12 - (as->as_stackpbase-(vaddr&PAGE_FRAME))/PAGE_SIZE;
        *pte = array_get(as->as_stackSeg,index);
    }
    else{
        //not in any segment
        return -1;
    }
    
    if((*pte)->flag & VALID && (*pte)->cm_index>=0) {
        // load the page into a frame/TLB
        return 0;
    } else {
        //raise page fault
        if(seg_type == TEXT || seg_type == DATA){
            return 1;
        }
        else{
            return 2;
        }
    }
}

int load_page(paddr_t paddr, vaddr_t vaddr){
    struct addrspace* as = curproc->p_addrspace;
    struct coremap_entry** cmt = get_global_coremap(); 
    int cm_index = (paddr - cmt[0]->cm_paddr)/PAGE_SIZE;
    vaddr_t page_number = vaddr&PAGE_FRAME;
    int seg_type = segment_type(page_number);
    if(cmt[cm_index]->cm_paddr != paddr){
        //error!!!!! should be equal
        return -1;
    }
    
    int i;
    
    switch (seg_type){
        case TEXT:
        {
            i = (page_number - as->as_vbase1)/PAGE_SIZE;
            struct pt_entry* pte = array_get(as->as_textSeg,i);
            change_pte(pte, cm_index, VALID);
            pte->flag &= ~DIRTY;
            set_coremap_proc(cm_index,TEXT);
            break;   
        }
        case DATA:
        {
            i = (page_number - as->as_vbase2)/PAGE_SIZE;
            struct pt_entry* pte = array_get(as->as_dataSeg,i);
            change_pte(pte, cm_index, DIRTY|VALID);
            set_coremap_proc(cm_index,DATA);
            break;   
        }
        case STACK:
        {
            i = 12 - (as->as_stackpbase-vaddr)/PAGE_SIZE;
            struct pt_entry* pte = array_get(as->as_stackSeg,i);
            change_pte(pte, cm_index, DIRTY|VALID);
            set_coremap_proc(cm_index,STACK);
            break;   
        }
    }
    
    return 0;
}


int page_fault(int faulttype, vaddr_t faultaddress) {
	int ret;
	switch (faulttype) {
		case COPY_FROM_ELF:
		{
            vmstats_inc(VMSTAT_PAGE_FAULT_DISK);
            vmstats_inc(VMSTAT_ELF_FILE_READ);
            ret = On_Demand_Loading(curproc_getas()->elf_vnode, faultaddress);
            break;
		}
		
		case COPY_FROM_SWAP:// faulttype:1
		{
            vmstats_inc(VMSTAT_PAGE_FAULT_DISK);
            vmstats_inc(VMSTAT_SWAP_FILE_READ);
            // ADD THIS FUNCTION
            // ret = write_to_swap(faultaddress);
            struct pt_entry* pte;
            get_pt_entry(&pte, faultaddress);
            ret = read_from_swap(pte);
            break;
		}
			
		case COPY_TO_STACK:
		{
			vmstats_inc(VMSTAT_PAGE_FAULT_ZERO);
            //vmstats_inc(VMSTAT_PAGE_FAULT_DISK);
            ret = stack_loading(faultaddress);
            vmstats_inc(VMSTAT_TLB_FAULT);
            vmstats_inc(VMSTAT_TLB_FAULT_FREE);
			if (ret) {
				return ret;
			}
			break;
		}
	}
	return ret;
}
#endif /* OPT_A3 */
