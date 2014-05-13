#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include <types.h>
#include <array.h>
#include <pt.h>
#include "opt-A3.h"



#if OPT_A3
#define SWAPFILE_SIZE 0x900000

struct swapfile_entry {
	vaddr_t vaddr;
	int32_t index;
	struct addrspace * in_addrspace; /* addrspace related to */
};

extern struct File* swapfile;

struct File* get_global_swapfile(void);
int read_from_swap(struct pt_entry * pte);
int write_to_swap(struct pt_entry * pte);
struct pt_entry* Pvictim(struct addrspace* as, int segment);

#endif /* OPT_A3 */
#endif /* _SWAPFILE_H_ */
