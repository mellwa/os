
#ifdef UW
/* This was added just to see if we can get things to compile properly and
 * to provide a bit of guidance for assignment 3 */

#include "opt-vm.h"
#if OPT_VM
#include <types.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <kern/errno.h>
#include <current.h>
#include <proc.h>
#include <spinlock.h>
#include <spl.h>
#include <mips/tlb.h>
#include <thread.h>
#include <coremap.h>
#include <pt.h>
#include <syscall.h>
#include <swapfile.h>
#include "opt-A3.h"
#include "uw-vmstats.h"


#if OPT_A3
#define DUMBVM_STACKPAGES    12
#define BASE_KADDR			0x80000000

struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

unsigned int next_victim = 0;
static int tlb_get_rr_victim(void);
void print_TLB(void);
static int tlb_get_rr_victim(){
    int victim;
    victim = next_victim;
    next_victim = (next_victim+1)%NUM_TLB;
    return victim;
}


void vm_shutdown(void) {
	_vmstats_print();
}

#endif

void
vm_bootstrap(void)
{
	/* May need to add code. */
#if OPT_A3
	// this will initialize the static coremap
    struct coremap_entry **coremap;
    coremap = get_global_coremap();
#endif
}

#if 0 
/* You will need to call this at some point */
//static

#endif

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
   /* Adapt code form dumbvm or implement something new */
#if OPT_A3
    paddr_t pa;
	pa = getppages(npages, false,9);
	if (pa==0) {
		return 0;
	}
	
	// kprintf("alloc paddr %d\n", pa);
	// kprintf("alloc vaddr %u\n", PADDR_TO_KVADDR(pa));
	
	return PADDR_TO_KVADDR(pa);
#else
	 (void)npages;
	 panic("Not implemented yet.\n");
   return (vaddr_t) NULL;
#endif
}

void 
free_kpages(vaddr_t addr)
{
#if OPT_A3
	// direct translation
	paddr_t paddr = addr - BASE_KADDR;
	
	if(paddr < lo_paddr) {
		// allocated using ram_stealmem()
		return;
	}
	
	struct coremap_entry ** coremap = get_global_coremap();
	
	unsigned int index = (paddr - lo_paddr) / PAGE_SIZE;
	
	KASSERT(coremap[index]->cm_proc == curproc);
	
	unsigned int length = coremap[index]->cm_length;
	
	// free the coremap frames
	for(unsigned int i = index; i < index + length; i++) {
		coremap[i]->cm_occupied = false;
		coremap[i]->cm_proc = NULL;
		coremap[i]->cm_length = 0;
        coremap[i]->seg_type = 0;
        as_zero_region(coremap[i]->cm_paddr, 1);
	}
	
	
#else
	/* nothing - leak the memory. */

	(void)addr;
#endif // OPT_A3
}

void
vm_tlbshootdown_all(void)
{
	panic("Not implemented yet.\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("Not implemented yet.\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
  /* Adapt code form dumbvm or implement something new */
#if OPT_A3
	paddr_t paddr;
	struct addrspace *as;
    int result;
    struct coremap_entry ** coremap;
    coremap  = get_global_coremap();
    struct pt_entry* pte;
    int p_fault;
	faultaddress &= PAGE_FRAME;
    
	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);
	switch (faulttype) {
	    case VM_FAULT_READONLY:
            p_fault = get_pt_entry(&pte, faultaddress);
            if(p_fault==1){
                //page fault
                // find a free frame and copy fauladdress into memory                
                result = page_fault(2,faultaddress);
                if(result){
                    return result;
                }
                return 0;
            }
            else if(p_fault == 2){
                //stack page fault should to call load stack
                result = page_fault(3,faultaddress);
                if(result){
                    return result;
                }
                paddr = coremap[pte->cm_index]->cm_paddr;
            }
            else if(p_fault==-1){
                //no such a segment in address space
                return EFAULT;
            }
            else{
	            vmstats_inc(VMSTAT_TLB_RELOAD);
                paddr = coremap[pte->cm_index]->cm_paddr; // get the frame from pt
            }
            vmstats_inc(VMSTAT_TLB_FAULT);
            vmstats_inc(VMSTAT_TLB_FAULT_FREE);
            if((pte->flag&VALID)&&(pte->flag&DIRTY)){
                result = TLB_updating(faultaddress, paddr);
                if(result){
                    return -1;
                }
                return 0;
            }
            if(!(pte->flag&MODIFIED)){
                if(pte->flag&VALID){
                    result = TLB_updating(faultaddress, paddr);
                    if(result){
                        return -1;
                    }
                    return 0;
                }
            }
            else{
                _exit(0);
            }
            //panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
            break;
	    default:
            return EINVAL;
	}
    
	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}
    
	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}
  
    //get page and handling page fault
    p_fault = get_pt_entry(&pte, faultaddress);
    if(p_fault==1){
        //page fault
        // find a free frame and copy fauladdress into memory
        if(pte->flag&IN_SWAP){
            result = page_fault(1,faultaddress);
        }
        else{
            result = page_fault(2,faultaddress);
            if(result){
                return result;
            }
            return 0;
        }
        if(result){
            return result;
        }
        paddr = coremap[pte->cm_index]->cm_paddr;
    }
    else if(p_fault == 2){
        //stack page fault should to call load stack
        if(pte->flag&IN_SWAP){
            result = page_fault(1,faultaddress);
        }
        else{
            result = page_fault(3,faultaddress);
        }
        if(result){
            return result;
        }
        paddr = coremap[pte->cm_index]->cm_paddr;
    }
    else if(p_fault==-1){
        //no such a segment in address space
        return EFAULT;
    }
    else{
        if(pte->cm_index == -1){
            kprintf("cm_index: %u\n",pte->cm_index);
        }
        vmstats_inc(VMSTAT_TLB_RELOAD);
        paddr = coremap[pte->cm_index]->cm_paddr; // get the frame from pt
    }
    
    
    result = TLB_updating(faultaddress, paddr);
    
    if(result){
        return -1;
    }
    return 0;
    
#else
	(void)faulttype;
	(void)faultaddress;
	panic("Not implemented yet.\n");
  return 0;
#endif
}

int TLB_updating(vaddr_t faultaddress, paddr_t paddr){
    unsigned int i;
    uint32_t ehi, elo;
	int spl;
    struct pt_entry* pte;
    int result;
    KASSERT((paddr & PAGE_FRAME) == paddr);
    faultaddress &= PAGE_FRAME;
	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();
    result = get_pt_entry(&pte, faultaddress);
    if(result && !(pte->flag & IN_SWAP)){
        //still page fault;
        splx(spl);
        return -1;
    }
    result = tlb_probe(faultaddress, 0);
    if(result >= 0){
        splx(spl);
        return 0;
    }
    vmstats_inc(VMSTAT_TLB_FAULT);
	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
        if((pte->flag & DIRTY) && ((pte->flag & VALID)||(pte->flag&IN_SWAP))){
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        }
        else if(((pte->flag & VALID)||(pte->flag&IN_SWAP))&& !(pte->flag & DIRTY)){
            if(pte->flag& MODIFIED){
                paddr &= ~TLBLO_DIRTY;
                elo = paddr  | TLBLO_VALID;
            }
            else{
                elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
            }
        }
        else{
            kprintf("The page table entry from get_pt_entry is invalid\n");
            splx(spl);
            return -1;
        }
        if(next_victim == i){
            next_victim += 1;
        }
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		vmstats_inc(VMSTAT_TLB_FAULT_FREE);
		tlb_write(ehi, elo, i);
        pte->flag |= MODIFIED;
		splx(spl);
		return 0;
	}
    i = tlb_get_rr_victim();
    ehi = faultaddress;
    if((pte->flag & DIRTY) && ((pte->flag & VALID)||(pte->flag&IN_SWAP))){
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
    }
    else if(((pte->flag & VALID)||(pte->flag&IN_SWAP))&& !(pte->flag & DIRTY)){
        if(pte->flag& MODIFIED){
            paddr &= ~TLBLO_DIRTY;
            elo = paddr  | TLBLO_VALID;
        }
        else{
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        }
    }
    DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
    vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);
    tlb_write(ehi,elo,i);
    splx(spl);
    return 0;
}


#if OPT_A3
// determine valid vaddrs
bool vm_invalidaddress(vaddr_t addr) {
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	struct addrspace *as;
	
	addr &= PAGE_FRAME;
	
	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}
	
	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}
	
	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;
	
	// check it is within the boundaries of a segment
	if (addr >= vbase1 && addr < vtop1) {
		paddr = (addr - vbase1) + as->as_pbase1;
	}
	else if (addr >= vbase2 && addr < vtop2) {
		paddr = (addr - vbase2) + as->as_pbase2;
	}
	else if (addr >= stackbase && addr < stacktop) {
		paddr = (addr - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}
	
	return 0;
}
#endif


paddr_t
getppages(unsigned long npages, bool swappable, int seg_type)
{
    /* Adapt code form dumbvm or implement something new */
#if OPT_A3
    paddr_t addr;
    
    if(coremap_exists()) {
	    addr = coremap_getFrames(npages, swappable,seg_type);
    } else {
		spinlock_acquire(&stealmem_lock);
		// steal mem before the coremap is allocated
		addr = ram_stealmem(npages);
		
		spinlock_release(&stealmem_lock);    
    }
    
    if(addr == 0){
        //coremap is full, need to get a victim to replace
        panic("*********| no address allocated |*********************\n");
        
    }
    
	return addr;
#else
    (void)npages;
    panic("Not implemented yet.\n");
    return (paddr_t) NULL;
#endif
}


// tlb debug
void print_TLB(){
    int i = 0;
    uint32_t ehi, elo;
    for(i = 0;i < 64;i++){
        tlb_read(&ehi, &elo, i);
        kprintf("%dth entry: page is %u   frame is %u\n",i,ehi,elo);
    }
    
}

#endif /* OPT_VM */

#endif /* UW */

