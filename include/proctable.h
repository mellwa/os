#ifndef _PROCTABLE_H_
#define _PROCTABLE_H_

#include <limits.h>
#include <types.h>
#include "opt-A2.h"
#include <proc.h>
#include <synch.h>

#if OPT_A2

struct ProcTable {
	struct proc * processes[OPEN_MAX];
	int num_running;
	struct cv * pt_cv;
	struct lock * pt_lock;
	struct lock * pt_lock2;
};

struct ProcTable * get_proctable(void);
struct proc * get_proc_by_pid(pid_t pid);
void add_proc_to_table(struct proc * p);
void remove_proc_from_table(struct proc * p);
bool can_add_to_proctable(void);

#endif /* OPT_A2 */
#endif /* _PROCTABLE_H_ */
