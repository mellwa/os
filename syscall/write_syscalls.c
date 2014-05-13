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
#include <limits.h>
#include <proc.h>
#include <filetable.h>
#include <current.h> // current thread defined
#include <kern/fcntl.h> // defined flags
#include <kern/iovec.h>
#include <uio.h>
#include <synch.h>
#include <vm.h>

#if OPT_A2
int write(unsigned int fd, const void *buffer, size_t len, int32_t *ret)
{
    struct File *tempfile = NULL;
    off_t data;
    struct iovec iov;
    struct uio u;
    int err;
	if(fd<=0 || fd >= OPEN_MAX){
        *ret = EBADF; 
        return -1;
    }
    
    if(vm_invalidaddress((vaddr_t)buffer)) {
        *ret = EFAULT;
        return -1;
    }
    
    if(!curproc->stdio_reserve){
        curproc->stdio_reserve = true;
        err = open("con:", O_RDONLY,ret);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,ret);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,ret);
        if(err){
            return -1;
        }
    }
    
    
    
    tempfile = curthread->t_proc->p_ft->files[fd];
    if(tempfile == NULL){
        *ret = EBADF;
        return -1;
    }
    if(tempfile->flags == O_RDONLY){
        *ret = EBADF;
        return -1;
    }
    
    lock_acquire(tempfile->rw_lock);
    
    uio_kinit(&iov, &u, (void*)buffer, len, tempfile->offset, UIO_WRITE);

    if(fd > 0){
        err = VOP_WRITE(tempfile->vn,&u);
        if(err){
            *ret = err;
            return -1;
        }
    }
    data = len - u.uio_resid;
    tempfile->offset = u.uio_offset;
    *ret = data;
    
    lock_release(tempfile->rw_lock);
	return 0;
}
#endif

