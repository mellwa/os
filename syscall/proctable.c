#include <proctable.h>
#include <lib.h>
#include <kern/errno.h>

#if OPT_A2

static struct ProcTable * GlobalProctable;

struct ProcTable * get_proctable(void) {
	if(!GlobalProctable) {
		GlobalProctable = kmalloc(sizeof(struct ProcTable));
		KASSERT(GlobalProctable != NULL);
		for (int i = 0; i < OPEN_MAX; i++) {
			GlobalProctable->processes[i] = NULL;
		}		
		GlobalProctable->num_running = 0;
		GlobalProctable->pt_cv = cv_create("proctable cv");
	    GlobalProctable->pt_lock = lock_create("proctable lock");
		GlobalProctable->pt_lock2 = lock_create("proclock2");
	}

	return GlobalProctable;
}

struct proc * get_proc_by_pid(pid_t pid) {
	struct ProcTable * proctable = get_proctable();
	KASSERT(proctable != NULL);
	if (pid == 0 || pid > OPEN_MAX) {
		return NULL;
	}
	return proctable->processes[pid];
}

void add_proc_to_table(struct proc * p) {
	struct ProcTable * proctable = get_proctable();
	KASSERT(proctable != NULL);
	KASSERT(p != NULL);
	// reserve pid=0 for fork()
	for (int i = 1; i < OPEN_MAX; i++) {
		if(proctable->processes[i] == NULL) {
			proctable->processes[i] = p;
			p->p_pid = i;
			// kprintf("Set pid: %d\n", i);
			// lock_acquire(proctable->pt_lock);
			 //  proctable->num_running++;
			// lock_release(proctable->pt_lock);
			return;
		}
	}
}

void remove_proc_from_table(struct proc * p) {
	struct ProcTable * proctable = get_proctable();
	KASSERT(proctable != NULL);
	if(p->p_pid < OPEN_MAX) {
		if(proctable->processes[p->p_pid] == p) {
			proctable->processes[p->p_pid] = NULL;
			//proctable->num_running--;
			return;
		}
	}
}

bool can_add_to_proctable(void) {
	for (int i = 0; i < OPEN_MAX; i++) {
		if(GlobalProctable->processes[i] != NULL) {
			return true;
		}
	}
	return false;
}

#endif /* OPT_A2 */
