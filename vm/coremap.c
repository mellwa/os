#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <coremap.h>
#include <vm.h>
#include <synch.h>
#include <mainbus.h>
#include <current.h>
#include <pt.h>
#include <swapfile.h>
#include <addrspace.h>
#include "opt-A3.h"

#if OPT_A3

static struct coremap_entry ** global_coremap;
// keep track of addresses available
paddr_t lo_paddr, hi_paddr;

// keep track of max_pages to iterate through coremap
uint32_t max_pages;

struct lock * coremap_lk;

struct coremap_entry ** get_global_coremap(void) {
	if(global_coremap) {
		return global_coremap;
	}
	
	coremap_lk = lock_create("coremap lock");

	// get max number of pages (overestimation)
	max_pages = mainbus_ramsize() / PAGE_SIZE;
	// allocate the coremap
	global_coremap = kmalloc(max_pages * sizeof(struct coremap_entry *));
	
	// lo is the min paddr, hi is the max paddr
	ram_getsize(&lo_paddr, &hi_paddr);
	
	// find the actual number of pages
	uint32_t num_pages = (hi_paddr - lo_paddr) / PAGE_SIZE;
	max_pages = num_pages;

	for(uint32_t i = 0; i < num_pages; i++) {
		// initialize blank coremap entries
		global_coremap[i] = kmalloc(sizeof(struct coremap_entry));
		global_coremap[i]->cm_occupied = false;
		global_coremap[i]->cm_length = 0;
		global_coremap[i]->cm_proc = NULL;
		global_coremap[i]->cm_paddr = lo_paddr;
        global_coremap[i]->seg_type = 0;

		// increment the address
		lo_paddr += PAGE_SIZE;
	}
	
	return global_coremap;
}

// debug function
void printCoremap(void) {
	struct coremap_entry ** coremap = get_global_coremap();
	
	kprintf("OUTPUT COREMAP START\n");

	for(unsigned int i = 0; i < max_pages; i++) {
		kprintf("Index: %u\tPaddr:%u\tOccupied:%d  seg type:%d\n", i, coremap[i]->cm_paddr, coremap[i]->cm_occupied,coremap[i]->seg_type);
	}
	
	kprintf("OUTPUT COREMAP COMPLETE\n");
}

void set_coremap_proc(unsigned int index, int seg_type){
    struct coremap_entry** coremap = get_global_coremap();
    if(index >= max_pages) return;
    // give a process a coremap entry
    coremap[index]->cm_proc = curproc;
    // set it to be occupied
    global_coremap[index]->cm_occupied = true;
    // set the segment type
    global_coremap[index]->seg_type = seg_type;
}

bool coremap_exists(void) {
	// check if we have allocated the coremap already
	return global_coremap != NULL;
}

paddr_t coremap_getFrames(unsigned long n, bool swappable, int seg_type) {
	paddr_t paddr;
    paddr = 0;
	// requesting too many pages
	if(n*PAGE_SIZE + lo_paddr > hi_paddr) {
        paddr = 0;
	}
    (void) seg_type;
	
	struct coremap_entry ** coremap = get_global_coremap();
	
	// grab lock for synchronization
	lock_acquire(coremap_lk);

	for(unsigned int i = 0; i < max_pages; i++) {
		if(!coremap[i]->cm_occupied) {
			// set in page table global_coremap[i];
			bool lengthFree = true;
			// try and find n continous frames
			for(unsigned int j = i + 1; (j < i + n) && (j < max_pages); j++) {
				// if its not n continous frames
				if(coremap[j]->cm_occupied) {
					lengthFree = false;
					break;
				}
			}
			
			// after found n continous frames, allocate them
			if(lengthFree && (i + n < max_pages)) {
				paddr = coremap[i]->cm_paddr;
				for(unsigned int j = i; j < i + n; j++) {
					coremap[j]->cm_occupied = true;
					coremap[j]->cm_length = n - (j - i);
					coremap[j]->cm_proc = curproc;
					coremap[j]->cm_swappable = swappable;
                    coremap[j]->seg_type = seg_type;
				}
				lock_release(coremap_lk);
                return paddr;
			}
		}
	}
	
	// if paddr = 0 at this point, we want to evict something
	// from physical memory
    struct addrspace *as;
    as = curproc_getas();
    if(as == NULL) {
	    // debug
	    // printCoremap();
    }
    
    // else the coremap is full
    struct pt_entry* pte;
	if(paddr == 0){
		// find a page victim
        pte = Pvictim(as, seg_type);
        // claim it
        coremap[pte->cm_index]->cm_proc = curproc;
        coremap[pte->cm_index]->cm_swappable = swappable;
        
        paddr = coremap[pte->cm_index]->cm_paddr;
        lock_release(coremap_lk);
        // write it to the swapfile
        write_to_swap(pte);
    }
    
	return paddr;
}

#endif /* OPT_A3 */
