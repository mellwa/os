/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <lib.h>
#include <vm.h>
#include <addrspace.h>
#include <kern/fcntl.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <mips/tlb.h>
#include <kern/errno.h>
#include <proc.h>
#include <pt.h>
#include <vfs.h>
#include <coremap.h>
#include <vfs.h>
#ifdef UW
#include <proc.h>
#endif
#include "opt-A3.h"
#include <uw-vmstats.h>
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */


#if OPT_A3
#define DUMBVM_STACKPAGES    12


//static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}
#endif



#if OPT_A3
struct addrspace *
as_create(char * path)
#else
struct addrspace *
as_create(void)
#endif
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
#if OPT_A3
    as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
    
    as->as_textSeg = array_create();
    if(as->as_textSeg == NULL){
        return NULL;
    }
    
    as->as_dataSeg = array_create();
    if(as->as_dataSeg == NULL){
        return NULL;
    }
    
    as->as_stackSeg = array_create();
    if(as->as_stackSeg == NULL){
        return NULL;
    }
    
    char *progname = kstrdup(path);
    as->progname = progname;
    vfs_open(progname, O_RDONLY, 0, &(as->elf_vnode));
    
#endif
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;
	#if OPT_A3
	newas = as_create(old->progname);
	#else
	newas = as_create();
	#endif
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */
#if OPT_A3
    newas->as_vbase1 = old->as_vbase1;
	newas->as_npages1 = old->as_npages1;
	newas->as_vbase2 = old->as_vbase2;
	newas->as_npages2 = old->as_npages2;
    
	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(newas)) {
		as_destroy(newas);
		return ENOMEM;
	}
    
	KASSERT(newas->as_pbase1 != 0);
	KASSERT(newas->as_pbase2 != 0);
	KASSERT(newas->as_stackpbase != 0);
    
	memmove((void *)PADDR_TO_KVADDR(newas->as_pbase1),
            (const void *)PADDR_TO_KVADDR(old->as_pbase1),
            old->as_npages1*PAGE_SIZE);
    
	memmove((void *)PADDR_TO_KVADDR(newas->as_pbase2),
            (const void *)PADDR_TO_KVADDR(old->as_pbase2),
            old->as_npages2*PAGE_SIZE);
    
	memmove((void *)PADDR_TO_KVADDR(newas->as_stackpbase),
            (const void *)PADDR_TO_KVADDR(old->as_stackpbase),
            DUMBVM_STACKPAGES*PAGE_SIZE);
#endif
	
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
#if OPT_A3
	struct coremap_entry** coremap;
	coremap = get_global_coremap();
	int index;
	struct pt_entry* pte;
    if(as==NULL){
        return;
    }
    //cleanup frames which load text segment
	for(index = 0; (unsigned)index < as->as_npages1; index++) {
		pte = array_get(as->as_textSeg,index);
        if(pte->flag&VALID && pte->cm_index >= 0){
			if(coremap[pte->cm_index] == NULL) {
				continue;
			}
            coremap[pte->cm_index]->cm_occupied = false;
            coremap[pte->cm_index]->cm_proc = NULL;
            coremap[pte->cm_index]->cm_length = 0;
            coremap[pte->cm_index]->seg_type = 0;
            as_zero_region(coremap[pte->cm_index]->cm_paddr, 1);
        }
	}
    
    for(index = as->as_npages1-1; index >= 0;index--){
        array_remove(as->as_textSeg,(unsigned)index);
    }
    array_destroy(as->as_textSeg);
    
    //cleanup frames which load data segment
    for(index = 0; (unsigned)index < as->as_npages2; index++) {
		pte = array_get(as->as_dataSeg,index);
        if(pte->flag&VALID && pte->cm_index >= 0){
			if(coremap[pte->cm_index] == NULL) {
				continue;
			}
            coremap[pte->cm_index]->cm_occupied = false;
            coremap[pte->cm_index]->cm_proc = NULL;
            coremap[pte->cm_index]->cm_length = 0;
            coremap[pte->cm_index]->seg_type = 0;
            as_zero_region(coremap[pte->cm_index]->cm_paddr, 1);
        }
	}
    
    for(index = as->as_npages2-1; index >= 0;index--){
        array_remove(as->as_dataSeg,(unsigned)index);
    }
    array_destroy(as->as_dataSeg);

    //cleanup frames which load stack segment
    for(index = 0; index < DUMBVM_STACKPAGES; index++) {
		pte = array_get(as->as_stackSeg,index);
        if(pte->flag&VALID && pte->cm_index >= 0){
			if(coremap[pte->cm_index] == NULL) {
				continue;
			}
            coremap[pte->cm_index]->cm_occupied = false;
            coremap[pte->cm_index]->cm_proc = NULL;
            coremap[pte->cm_index]->cm_length = 0;
            coremap[pte->cm_index]->seg_type = 0;
            as_zero_region(coremap[pte->cm_index]->cm_paddr, 1);
        }
	}
    for(index = DUMBVM_STACKPAGES-1; index >= 0;index--){
        array_remove(as->as_stackSeg,(unsigned)index);
    }
    array_destroy(as->as_stackSeg);
        

    //*********** FLUSH TLB **********************
    int i, spl;
    /* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();
    
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	
	vmstats_inc(VMSTAT_TLB_INVALIDATE);
    
	splx(spl);
    vfs_close(as->elf_vnode);
    //**********************************************

	kfree(as);
#endif
}

void
as_activate(void)
{
	struct addrspace *as;

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
#if OPT_A3
    int i, spl;
    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();
    
    // invalidate the TLB
    for (i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    
    vmstats_inc(VMSTAT_TLB_INVALIDATE);
    splx(spl);
#endif
}

void
#ifdef UW
as_deactivate(void)
#else
as_dectivate(void)
#endif
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */
#if OPT_A3
    unsigned int i;
    size_t npages; 
    
	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;
    
	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;
    
	npages = sz / PAGE_SIZE;
    
	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;
	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
        struct pt_entry* pte;
        array_setsize(as->as_textSeg,npages);
        for(i = 0; i<npages; i++){
            pte = build_ptentry(-1,vaddr+i*PAGE_SIZE,0/*readable|writeable|executable*/);
            array_set(as->as_textSeg,i,pte);
        }
		return 0;
	}
    
	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
        struct pt_entry* pte;
        array_setsize(as->as_dataSeg,npages);
        for(i = 0; i<npages; i++){
            pte = build_ptentry(-1,vaddr+i*PAGE_SIZE,0/*readable|writeable|executable*/);
            array_set(as->as_dataSeg,i,pte);
        }
		return 0;
	}
    
	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
#endif
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
#if OPT_A3
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);
    
    // allocate npages for text
	as->as_pbase1 = getppages(as->as_npages1, true,TEXT);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}
    
    // allocate npages for data
	as->as_pbase2 = getppages(as->as_npages2, true,DATA);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}
	
    // allocate npages for stack
	as->as_stackpbase = getppages(DUMBVM_STACKPAGES, true,STACK);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
#else
	(void)as;
#endif
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */
#if OPT_A3
    unsigned int i;
    if(as->as_stackpbase == 0){
        as->as_stackpbase = USERSTACK;
    }

    vaddr_t startpoint = USERSTACK;
    startpoint -= PAGE_SIZE*DUMBVM_STACKPAGES;
    struct pt_entry* pte;
    array_setsize(as->as_stackSeg,DUMBVM_STACKPAGES);
    for(i = 0; i<DUMBVM_STACKPAGES; i++){
        pte = build_ptentry(-1,startpoint+i*PAGE_SIZE,0);
        array_set(as->as_stackSeg,i,pte);
    }
#endif

	(void)as;
    KASSERT(as->as_stackpbase != 0);
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	
	return 0;
}

