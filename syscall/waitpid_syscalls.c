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
#include <synch.h>
#include <proc.h>
#include <proctable.h>

//int exitcode; 
//int is_exit; 
#if OPT_A2
pid_t waitpid(pid_t pid, int *status, int options, int32_t *ret) {

    if(pid <=0 || pid >= OPEN_MAX){
        *ret = ESRCH;
        return -1;
    }
    if(options != 0){//check for options
        *ret = EINVAL;
        return -1;
    }
    if (status == NULL || (unsigned int)status >= 0x80000000 || (unsigned int)status == 0x40000000 || (unsigned int)status % (sizeof(char *)) != 0) {
        *ret = EFAULT;
        return -1;
    }
    curproc->is_waiting = 1;
    if (curproc->child_is_exit) {
        *status = curproc->child_exitcode;
        *ret = pid;
        curproc->is_waiting = 0;
        return 0;
    }
    pid_t parent_pid = (pid_t)curproc->p_pid;
    struct proc * child = get_proc_by_pid(pid);
    if (child == NULL) {
        *ret = ESRCH;
        return -1;
    }
    
    //make the currentproc only interested in waiting for its child.
    if (parent_pid != (pid_t)child->p_parentpid) {
        *ret = ECHILD;
        return  -1;
    }
    //child->exitcode_retrieved = 1;
    P(curproc->p_sem);
    KASSERT(curproc->child_is_exit);
    *status = curproc->child_exitcode;
    *ret = pid;
    return 0;
      
}
#endif

