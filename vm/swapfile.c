#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <vm.h>
#include <synch.h>
#include <mainbus.h>
#include <pt.h>
#include <current.h>
#include <addrspace.h>
#include <uio.h>
#include <kern/iovec.h>
#include <vnode.h>
#include <swapfile.h>
#include <array.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <filetable.h>
#include <proc.h>
#include <coremap.h>
#include <uw-vmstats.h>
#include <mips/tlb.h>
#include "opt-A3.h"

#if OPT_A3

/* split up swapfile into 2304 slots, each of size PAGE_SIZE and index each slot */
volatile bool offset_array[2304];
static struct File* global_swapfile;

//find an available slot for storing
static int find_slot(){
    for(int i=0;i<2304;i++){
        if(offset_array[i]){
            return i;
        }
    }
    return -1; //no more space in swapfile
}

//return a pointer to the global swapfile if there is one, or create a new swapfile otherwise 
struct File* get_global_swapfile(void) {
	if(global_swapfile != NULL) {
		return global_swapfile;
	}
	
	for(int i=0; i<2304; i++){
			offset_array[i] = true;
		}
		// allocate all substructures
		global_swapfile = kmalloc(sizeof(struct File));
		if(global_swapfile == NULL){
			return NULL;
		}
		global_swapfile->flags = 0;
		char* progname = kstrdup("SWAPFILE");
		if(progname == NULL){
			return NULL;
		}
		struct vnode * vn;
		int ret = vfs_open(progname, O_RDWR|O_CREAT|O_TRUNC,0,&vn);
		if (ret) {
			return NULL;
		}
		// set file properties
		global_swapfile->vn = vn;
		global_swapfile->flags |= O_RDWR|O_CREAT|O_TRUNC;
		global_swapfile->offset = 0;
		global_swapfile->rw_lock = lock_create("rw_lock");
		
		return global_swapfile;
}

int write_to_swap(struct pt_entry * pte) {
	struct File* swapfile = get_global_swapfile();
	//lock_acquire(swapfile->rw_lock);
	if (pte->flag & MODIFIED) {
		vmstats_inc(VMSTAT_SWAP_FILE_WRITE);
		// if it was in swapfile, find the previous slot and replace that slot
		if (pte->flag & IN_SWAP) { 
			off_t offset = pte->swapfile_index * PAGE_SIZE; 
			struct iovec iov;
			struct uio u;
			uio_kinit(&iov, &u, (void *)pte->page_number, PAGE_SIZE, offset, UIO_WRITE);
			int err;
			err = VOP_WRITE(swapfile->vn, &u);
			if (err) {
				kprintf("error 1 writing to swap\n");
				//lock_release(swapfile->rw_lock);
				return -1;
			}
            swapfile->offset = u.uio_offset;//set new offset in swapfile
            if(swapfile->offset > SWAPFILE_SIZE){
	           // lock_release(swapfile->rw_lock);
                kprintf("EXCEED THE SIZE OF SWAPFILE\n");
                return -1;
            }
            pte->flag &= ~VALID; //set the page table entry invalid
		} else {
            int index = find_slot();
            if(index == -1){
	            kprintf("error 2 writing to swap\n");
	           // lock_release(swapfile->rw_lock);
                return -1;
            }
			off_t offset = index * PAGE_SIZE;
			struct iovec iov;
			struct uio u;
			uio_kinit(&iov, &u, (void *)pte->page_number, PAGE_SIZE, offset, UIO_WRITE);
			
			int err;
			err = VOP_WRITE(swapfile->vn, &u);
			if (err) {
				kprintf("error 3 writing to swap\n");
				//lock_release(swapfile->rw_lock);
				return -1;
			}
			swapfile->offset = u.uio_offset;
            if(swapfile->offset > SWAPFILE_SIZE){
                kprintf("EXCEED THE SIZE OF SWAPFILE\n");
               //lock_release(swapfile->rw_lock);
                return -1;
            }
            pte->swapfile_index = index; //store the index into page table entry
            pte->flag |= IN_SWAP; // this page is in swapfile
            pte->flag &= ~VALID;
            offset_array[index] = false; //this position is not available anymore
		}
		
		//flush the corresponding TLB entry and free the page in memory
		struct coremap_entry ** coremap = get_global_coremap();
		
		vaddr_t vaddr = pte->page_number;
		paddr_t paddr = coremap[pte->cm_index]->cm_paddr;
		int i = tlb_probe(vaddr, paddr);
        if(i >= 0){
            tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }
        as_zero_region(paddr, 1);
	}
    else{
        kprintf("page:%u flag:%u\n",pte->page_number,pte->flag);
	    kprintf("error 4 writing to swap\n");
	    //lock_release(swapfile->rw_lock);
        return 0;
    }
   //lock_release(swapfile->rw_lock);
    
	return 0;
}


int read_from_swap(struct pt_entry * pte){
	struct File* swapfile = get_global_swapfile();
	off_t offset;
	struct iovec iov;
	struct uio u;
    int err;
    struct coremap_entry ** coremap;
    int frame_index;
    paddr_t paddr;
    int seg_type;
	lock_acquire(swapfile->rw_lock);
    
    //get a victim frame to load the page in pte
    coremap = get_global_coremap();
    offset = pte->swapfile_index * PAGE_SIZE;
    seg_type = segment_type(pte->page_number);//get the segment type
    paddr = getppages(1, true,seg_type);
    frame_index = (paddr - coremap[0]->cm_paddr)/PAGE_SIZE;
    coremap[frame_index]->cm_occupied = true;
    set_coremap_proc(frame_index,seg_type);
    pte->cm_index = frame_index;
    //******** UPDATE TLB ************
    uint32_t ehi, elo;
    int result;
    tlb_read(&ehi,&elo,result);
    result = tlb_probe(pte->page_number, 0);
    TLB_updating(pte->page_number, paddr);
    result = tlb_probe(pte->page_number, 0);
    tlb_read(&ehi,&elo,result);
    
    
    
    pte->flag |= VALID;
    //**********************************
    
	uio_kinit(&iov, &u, (void*)pte->page_number, PAGE_SIZE, offset, UIO_READ);
    
	err = VOP_READ(swapfile->vn, &u);
    if(err){
        //reading failed
        lock_release(swapfile->rw_lock);
        return -1;
    }
    pte->flag |= VALID;//set the page table entry to valid
	lock_release(swapfile->rw_lock);


    return 0;
}



struct pt_entry* Pvictim(struct addrspace* as, int segment){
    unsigned int i = 0;
    unsigned int j = 0;
    struct pt_entry* pte;
    if(as == NULL){
        //error
        return NULL;
    }
    (void)segment;
    while(1){
        if(segment == TEXT){
	        // grab an entry from data seg
            pte = array_get(as->as_dataSeg,i);
            // if it is in the coremap
            if(pte->cm_index >= 0 && (pte->flag&VALID)){
	            // it is eligible for removal
                return pte;
            }
            // else increment count
            i = (i+1)%array_num(as->as_dataSeg);
            if(i==0 && j>0){
                printCoremap();
                panic("STOP!\n");
            }
        }
        else if(segment == DATA){
	        // grab an entry from data seg
            pte = array_get(as->as_dataSeg,i);
            // if it is in the coremap
            if(pte->cm_index >= 0 && (pte->flag&VALID)){
	            // it is eligible for removal
                return pte;
            }
            // else increment count
            i = (i+1)%array_num(as->as_dataSeg);
            if(i==0 && j>0){
                printCoremap();
                panic("STOP!\n");
            }
        }
        else if(segment == STACK){
	        // grab an entry from stack seg
            pte = array_get(as->as_stackSeg,i);
            // if it is in the coremap
            if(pte->cm_index >= 0 && (pte->flag&VALID)){
	            // it is eligible for removal
                return pte;
            }
            i = (i+1)%array_num(as->as_stackSeg);//get the next pte
            if(i==0 && j>0){
                printCoremap();
                panic("STOP!\n");
            }
        }
        j++;
    }
        return NULL;
}


#endif
