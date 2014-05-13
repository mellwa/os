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
#include "opt-A2.h"
#include <types.h>
#include <thread.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <current.h>
#include <proc.h>
#include <proctable.h>
#include <synch.h>
#include <kern/wait.h>

#if OPT_A2
void _exit(int exitcode){

    struct proc * parent = get_proc_by_pid(curproc->p_parentpid);
    if(parent != NULL){
        exitcode = _MKWAIT_EXIT(exitcode);
        parent->child_exitcode = exitcode;
        parent->child_is_exit = 1;
        //call V only when parent is waiting
        if (parent->is_waiting) {
            parent->is_waiting = 0;
            V(parent->p_sem);
        }
    }
    
//    struct ProcTable * pt = get_proctable();
//    lock_acquire(pt->pt_lock);
//    pt->num_running--;
//    cv_signal(pt->pt_cv, pt->pt_lock);
//    lock_release(pt->pt_lock);
//
//	sem_destroy(curproc->p_sem);
   thread_exit();
}
#endif

