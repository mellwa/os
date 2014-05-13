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


/*
 * Code to load an ELF-format executable into the current address space.
 *
 * It makes the following address space calls:
 *    - first, as_define_region once for each segment of the program;
 *    - then, as_prepare_load;
 *    - then it loads each chunk of the program;
 *    - finally, as_complete_load.
 *
 * This gives the VM code enough flexibility to deal with even grossly
 * mis-linked executables if that proves desirable. Under normal
 * circumstances, as_prepare_load and as_complete_load probably don't
 * need to do anything.
 *
 * If you wanted to support memory-mapped executables you would need
 * to rearrange this to map each segment.
 *
 * To support dynamically linked executables with shared libraries
 * you'd need to change this to load the "ELF interpreter" (dynamic
 * linker). And you'd have to write a dynamic linker...
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <elf.h>
#include <vm.h>
#include <spl.h>
#include <pt.h>
#include <mips/tlb.h>
#include "opt-A3.h"
#include <uw-vmstats.h>


/*
 * Load a segment at virtual address VADDR. The segment in memory
 * extends from VADDR up to (but not including) VADDR+MEMSIZE. The
 * segment on disk is located at file offset OFFSET and has length
 * FILESIZE.
 *
 * FILESIZE may be less than MEMSIZE; if so the remaining portion of
 * the in-memory segment should be zero-filled.
 *
 * Note that uiomove will catch it if someone tries to load an
 * executable whose load address is in kernel space. If you should
 * change this code to not use uiomove, be sure to check for this case
 * explicitly.
 */
static int tlb_get_rr_victim(void);
static int tlb_get_rr_victim(){
    int victim;
    victim = next_victim;
    next_victim = (next_victim+1)%NUM_TLB;
    return victim;
}

static
int
load_segment(struct addrspace *as, struct vnode *v,
	     off_t offset, vaddr_t vaddr, 
	     size_t memsize, size_t filesize,
	     int is_executable)
{
	struct iovec iov;
	struct uio u;
	int result;

#if OPT_A3
    struct uio u2;
    struct iovec iov2;
    iov2.iov_ubase = (userptr_t)vaddr;
	iov2.iov_len = PAGE_SIZE;		 // length of the memory space
	u2.uio_iov = &iov2;
	u2.uio_iovcnt = 1;
	u2.uio_resid = PAGE_SIZE;          // amount to read from the file
	u2.uio_offset = 0;
	u2.uio_segflg = is_executable ? UIO_USERISPACE : UIO_USERSPACE;
	u2.uio_rw = UIO_READ;
	u2.uio_space = as;
    
    result = uiomovezeros(PAGE_SIZE, &u2);
    
#endif
    
	if (filesize > memsize) {
		kprintf("ELF: warning: segment filesize > segment memsize\n");
		filesize = memsize;
	}

	DEBUG(DB_EXEC, "ELF: Loading %lu bytes to 0x%lx\n", 
	      (unsigned long) filesize, (unsigned long) vaddr);

	iov.iov_ubase = (userptr_t)vaddr;
	iov.iov_len = memsize;		 // length of the memory space
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = filesize;          // amount to read from the file
	u.uio_offset = offset;
	u.uio_segflg = is_executable ? UIO_USERISPACE : UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = as;

	result = VOP_READ(v, &u);
	if (result) {
		return result;
	}

	if (u.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on segment - file truncated?\n");
		return ENOEXEC;
	}

	/*
	 * If memsize > filesize, the remaining space should be
	 * zero-filled. There is no need to do this explicitly,
	 * because the VM system should provide pages that do not
	 * contain other processes' data, i.e., are already zeroed.
	 *
	 * During development of your VM system, it may have bugs that
	 * cause it to (maybe only sometimes) not provide zero-filled
	 * pages, which can cause user programs to fail in strange
	 * ways. Explicitly zeroing program BSS may help identify such
	 * bugs, so the following disabled code is provided as a
	 * diagnostic tool. Note that it must be disabled again before
	 * you submit your code for grading.
	 */
#if 1
		size_t fillamt;

		fillamt = memsize - filesize;
		if (fillamt > 0) {
			DEBUG(DB_EXEC, "ELF: Zero-filling %lu more bytes\n", 
			      (unsigned long) fillamt);
			u.uio_resid += fillamt;
			result = uiomovezeros(fillamt, &u);
		}
#endif
	return result;
}

/*
 * Load an ELF executable user program into the current address space.
 *
 * Returns the entry point (initial PC) for the program in ENTRYPOINT.
 */
int
load_elf(struct vnode *v, vaddr_t *entrypoint)
{
	Elf_Ehdr eh;   /* Executable header */
	Elf_Phdr ph;   /* "Program header" = segment header */
	int result, i;
	struct iovec iov;
	struct uio ku;
	struct addrspace *as;

	as = curproc_getas();

	/*
	 * Read the executable header from offset 0 in the file.
	 */

	uio_kinit(&iov, &ku, &eh, sizeof(eh), 0, UIO_READ);
	result = VOP_READ(v, &ku);
	if (result) {
		return result;
	}

	if (ku.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on header - file truncated?\n");
		return ENOEXEC;
	}

	/*
	 * Check to make sure it's a 32-bit ELF-version-1 executable
	 * for our processor type. If it's not, we can't run it.
	 *
	 * Ignore EI_OSABI and EI_ABIVERSION - properly, we should
	 * define our own, but that would require tinkering with the
	 * linker to have it emit our magic numbers instead of the
	 * default ones. (If the linker even supports these fields,
	 * which were not in the original elf spec.)
	 */

	if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
	    eh.e_ident[EI_MAG1] != ELFMAG1 ||
	    eh.e_ident[EI_MAG2] != ELFMAG2 ||
	    eh.e_ident[EI_MAG3] != ELFMAG3 ||
	    eh.e_ident[EI_CLASS] != ELFCLASS32 ||
	    eh.e_ident[EI_DATA] != ELFDATA2MSB ||
	    eh.e_ident[EI_VERSION] != EV_CURRENT ||
	    eh.e_version != EV_CURRENT ||
	    eh.e_type!=ET_EXEC ||
	    eh.e_machine!=EM_MACHINE) {
		return ENOEXEC;
	}

	/*
	 * Go through the list of segments and set up the address space.
	 *
	 * Ordinarily there will be one code segment, one read-only
	 * data segment, and one data/bss segment, but there might
	 * conceivably be more. You don't need to support such files
	 * if it's unduly awkward to do so.
	 *
	 * Note that the expression eh.e_phoff + i*eh.e_phentsize is 
	 * mandated by the ELF standard - we use sizeof(ph) to load,
	 * because that's the structure we know, but the file on disk
	 * might have a larger structure, so we must use e_phentsize
	 * to find where the phdr starts.
	 */

	for (i=0; i<eh.e_phnum; i++) {
		off_t offset = eh.e_phoff + i*eh.e_phentsize;
		uio_kinit(&iov, &ku, &ph, sizeof(ph), offset, UIO_READ);

		result = VOP_READ(v, &ku);
		if (result) {
			return result;
		}

		if (ku.uio_resid != 0) {
			/* short read; problem with executable? */
			kprintf("ELF: short read on phdr - file truncated?\n");
			return ENOEXEC;
		}

		switch (ph.p_type) {
		    case PT_NULL: /* skip */ continue;
		    case PT_PHDR: /* skip */ continue;
		    case PT_MIPS_REGINFO: /* skip */ continue;
		    case PT_LOAD: break;
		    default:
			kprintf("loadelf: unknown segment type %d\n", 
				ph.p_type);
			return ENOEXEC;
		}

		result = as_define_region(as,
					  ph.p_vaddr, ph.p_memsz,
					  ph.p_flags & PF_R,
					  ph.p_flags & PF_W,
					  ph.p_flags & PF_X);
		if (result) {
			return result;
		}
	}
#if OPT_A3
#else
	result = as_prepare_load(as);
	if (result) {
		return result;
	}

	/*
	 * Now actually load each segment.
	 */

	for (i=0; i<eh.e_phnum; i++) {
		off_t offset = eh.e_phoff + i*eh.e_phentsize;
		uio_kinit(&iov, &ku, &ph, sizeof(ph), offset, UIO_READ);

		result = VOP_READ(v, &ku);
		if (result) {
			return result;
		}

		if (ku.uio_resid != 0) {
			/* short read; problem with executable? */
			kprintf("ELF: short read on phdr - file truncated?\n");
			return ENOEXEC;
		}

		switch (ph.p_type) {
		    case PT_NULL: /* skip */ continue;
		    case PT_PHDR: /* skip */ continue;
		    case PT_MIPS_REGINFO: /* skip */ continue;
		    case PT_LOAD: break;
		    default:
			kprintf("loadelf: unknown segment type %d\n", 
				ph.p_type);
			return ENOEXEC;
		}

		result = load_segment(as, v, ph.p_offset, ph.p_vaddr, 
				      ph.p_memsz, ph.p_filesz,
				      ph.p_flags & PF_X);
		if (result) {
			return result;
		}
	}

	result = as_complete_load(as);
	if (result) {
		return result;
	}
#endif

	*entrypoint = eh.e_entry;

	return 0;
}

#if OPT_A3
int On_Demand_Loading(struct vnode *v, vaddr_t vaddr){
    Elf_Ehdr eh;   /* Executable header */
	Elf_Phdr ph;   /* "Program header" = segment header */
	int result, i;
	struct iovec iov;
	struct uio ku;
	struct addrspace *as;
    paddr_t paddr;
    int seg_type;
    uint32_t ehi, elo;
    
	as = curproc_getas();
    
	/*
	 * Read the executable header from offset 0 in the file.
	 */
    
	uio_kinit(&iov, &ku, &eh, sizeof(eh), 0, UIO_READ);
	result = VOP_READ(v, &ku);
	if (result) {
		return result;
	}
    
	if (ku.uio_resid != 0) {
		/* short read; problem with executable? */
		kprintf("ELF: short read on header - file truncated?\n");
		return ENOEXEC;
	}
    
	/*
	 * Check to make sure it's a 32-bit ELF-version-1 executable
	 * for our processor type. If it's not, we can't run it.
	 *
	 * Ignore EI_OSABI and EI_ABIVERSION - properly, we should
	 * define our own, but that would require tinkering with the
	 * linker to have it emit our magic numbers instead of the
	 * default ones. (If the linker even supports these fields,
	 * which were not in the original elf spec.)
	 */
    
	if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
	    eh.e_ident[EI_MAG1] != ELFMAG1 ||
	    eh.e_ident[EI_MAG2] != ELFMAG2 ||
	    eh.e_ident[EI_MAG3] != ELFMAG3 ||
	    eh.e_ident[EI_CLASS] != ELFCLASS32 ||
	    eh.e_ident[EI_DATA] != ELFDATA2MSB ||
	    eh.e_ident[EI_VERSION] != EV_CURRENT ||
	    eh.e_version != EV_CURRENT ||
	    eh.e_type!=ET_EXEC ||
	    eh.e_machine!=EM_MACHINE) {
		return ENOEXEC;
	}
    
	/*
	 * Go through the list of segments and set up the address space.
	 *
	 * Ordinarily there will be one code segment, one read-only
	 * data segment, and one data/bss segment, but there might
	 * conceivably be more. You don't need to support such files
	 * if it's unduly awkward to do so.
	 *
	 * Note that the expression eh.e_phoff + i*eh.e_phentsize is 
	 * mandated by the ELF standard - we use sizeof(ph) to load,
	 * because that's the structure we know, but the file on disk
	 * might have a larger structure, so we must use e_phentsize
	 * to find where the phdr starts.
	 */
    uint32_t newPhoffset;
    vaddr_t vaddr_base;
    int j;
    int spl;
    seg_type = segment_type(vaddr);
    paddr = getppages(1, true,seg_type);
	for (i=0; i<eh.e_phnum; i++) {
		off_t offset = eh.e_phoff + i*eh.e_phentsize;
		uio_kinit(&iov, &ku, &ph, sizeof(ph), offset, UIO_READ);
        
		result = VOP_READ(v, &ku);
		if (result) {
			return result;
		}
        
		if (ku.uio_resid != 0) {
			/* short read; problem with executable? */
			kprintf("ELF: short read on phdr - file truncated?\n");
			return ENOEXEC;
		}
        
		switch (ph.p_type) {
		    case PT_NULL: /* skip */ continue;
		    case PT_PHDR: /* skip */ continue;
		    case PT_MIPS_REGINFO: /* skip */ continue;
		    case PT_LOAD: 
                seg_type = segment_type(vaddr);
                if(ph.p_vaddr == as->as_vbase1 && seg_type == TEXT){
                    newPhoffset = ph.p_offset + (vaddr - as->as_vbase1);
                    vaddr_base = as->as_vbase1;
                }
                else if(ph.p_vaddr == as->as_vbase2 && seg_type == DATA){
                    newPhoffset = ph.p_offset + (vaddr - as->as_vbase2);
                    vaddr_base = as->as_vbase2;
                }
                else{
                    //kprintf("no such a segment type\n");
                    continue;
                }
                break;
		    default:
                kprintf("loadelf: unknown segment type %d\n", 
                        ph.p_type);
                return ENOEXEC;
		}
        result = load_page(paddr,vaddr);// paddr loads page vaddr 
        if(result == -1){
            //page loading error
            return result;
        }
        spl = splhigh();
        
        j = tlb_probe(vaddr, 0);
        //update tlb immediately
        vmstats_inc(VMSTAT_TLB_FAULT);
        if(j >= 0){
            tlb_read(&ehi, &elo, j);
            ehi = vaddr;
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
            tlb_write(ehi,elo,j);
            vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);
        }
        else{
            while(1){
                j = tlb_get_rr_victim();
                tlb_read(&ehi, &elo, j);
                if (!(elo & TLBLO_VALID)) {
                    break;
                }
            }
            vmstats_inc(VMSTAT_TLB_FAULT_FREE);
           
            ehi = vaddr;
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
            DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", vaddr, paddr);
           
            tlb_write(ehi,elo,j);
        }
        splx(spl);
        
        if(ph.p_filesz < vaddr - vaddr_base){
            for(i=0; i<PAGE_SIZE;i++){
                // *((int*)vaddr+i) = 0;
            }
        }
        else if(PAGE_SIZE >= ph.p_filesz - (newPhoffset - ph.p_offset)){
            //load data into the page
            result = load_segment(as, v, newPhoffset, vaddr, 
                                  PAGE_SIZE, ph.p_filesz - (newPhoffset - ph.p_offset),
                                  ph.p_flags & PF_X);
        }
        else{
            result = load_segment(as, v, newPhoffset,vaddr, 
                                  PAGE_SIZE, PAGE_SIZE,
                                  ph.p_flags & PF_X);
        }
        //check if load segment succeed
        if(result){
            return result;
        }
        struct pt_entry *pte;
        result = get_pt_entry(&pte, vaddr);
        if(result !=0){
            kprintf("cannot have a page pault here!\n");
        }
        else{
            pte->flag |= MODIFIED;
        }
        if(!(ph.p_flags&PF_W)){
            spl = splhigh();
            j = tlb_probe(vaddr, 0);
            if(j <0){
                kprintf("IMPOSSIBLE!!!!!!\n");
            }
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
            elo &= ~TLBLO_DIRTY; // set to non-writable since the segment is readonly
            tlb_write(ehi,elo,j);
            splx(spl);
        }
    }
    
    
    return 0;
}

//load stack set all page zero
int stack_loading(vaddr_t vaddr){
    paddr_t paddr;
    struct addrspace *as;
    as = curproc_getas();
    
    int ret;
    
    struct iovec iov;
	struct uio u; // initialize uio struct
	iov.iov_ubase = (userptr_t)vaddr;
	iov.iov_len = PAGE_SIZE;		 // length of the memory space
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = PAGE_SIZE;          // amount to read from the file
	u.uio_offset = 0;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = as;
    int seg_type = segment_type(vaddr);
    paddr = getppages(1, true,seg_type);
    
    ret = load_page(paddr, vaddr);// load the page
    if(ret){
        return ret;
    }
    ret = uiomovezeros(PAGE_SIZE, &u);//set a frame to all zero
    if(ret){
        return ret;
    }
    return 0;
}

#endif
