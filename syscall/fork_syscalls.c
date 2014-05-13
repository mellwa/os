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
#include <copyinout.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <current.h>
#include <thread.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <kern/syscall.h>
#include <proc.h>
#include <proctable.h>

#if OPT_A2

// forward declaration

struct trapframe * trapframe_duplicate(struct trapframe * trapframe);

struct trapframe * trapframe_duplicate(struct trapframe * trapframe) {
    struct trapframe * dup = kmalloc(sizeof(struct trapframe));
    if(dup == NULL) {
        return NULL;
    }

    memcpy(dup, trapframe, sizeof(struct trapframe));
    
    return dup;
}

int fork(struct trapframe * parent_trapframe, int32_t *ret) {
    struct proc *parent_proc = curproc;
    
    if(!can_add_to_proctable()) {
        *ret = ENPROC;
        return -1;
    }
        
    struct proc * child_proc = maybe_proc_create(parent_proc->p_name);
    
    if(child_proc == NULL) {
        *ret = ENOMEM;
        return -1;
    }

    int result = proc_duplicate(parent_proc, child_proc);
    if(result != 0) {
        *ret = result;
        return -1;
    }
    void * child_trapframe = trapframe_duplicate(parent_trapframe);
    get_proctable()->num_running++;
    result = thread_fork(curthread->t_name, child_proc, enter_forked_process, child_trapframe, (unsigned int) parent_proc->p_addrspace);
    // handle error here
    (void)result;
    
    *ret = child_proc->p_pid;
    
    return 0;
}

#endif

