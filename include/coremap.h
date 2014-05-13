#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <proc.h>
#include "opt-A3.h"

#if OPT_A3

struct coremap_entry {
	paddr_t cm_paddr;
	bool cm_occupied;
	// keep track of how many frames were allocated
	uint32_t cm_length;
	struct proc * cm_proc;
    bool cm_swappable;
    int seg_type;
};

paddr_t coremap_getFrames(unsigned long n, bool swappable,int seg_type);
struct coremap_entry ** get_global_coremap(void);
bool coremap_exists(void);
void set_coremap_proc(unsigned int index, int seg_type);
void printCoremap(void);

extern paddr_t lo_paddr, hi_paddr;

#endif /* OPT_A3 */
#endif /* _COREMAP_H_ */
