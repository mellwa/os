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
#include <vfs.h>
#include <filetable.h>
#include <current.h>
#include <vnode.h>
#include <proc.h>
#include <kern/fcntl.h>
#include <vm.h>

#if OPT_A2
int open(const char *filename, int flags, int32_t *ret){
    int result;
    int err;
    
    if(filename == NULL || (unsigned int)filename == 0x40000000) {
        *ret = EFAULT;
        return -1;
    }  
    //check if the filename address is in kernel
    if((unsigned int)filename >= 0x80000000){
        if ((unsigned int)filename < 0xffffffff){
            if(strcmp(filename,"con:")){
                    *ret = EFAULT;
                    return -1;
            }
        }
    }
    //check if the filename is invalid pointer
  
    if((flags & O_RDWR) && vm_invalidaddress((vaddr_t)filename)) {
        *ret = EFAULT;
        return -1;
    }
    
    if (strlen(filename) == 0) {
        * ret = EINVAL;
        return -1;
    }

    /* check valid combinations of flags */
    if (flags & O_APPEND) {
        *ret = EFAULT;
        return -1;
    }
    if (flags & O_EXCL) {
        if (!flags & O_CREAT) {
            *ret = EFAULT;
            return -1;
        }
    }
    //reserve 0,1,2 for stdio
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
    struct vnode *retvnode; /*  */ 
    char* tempFilename;

    tempFilename = kstrdup(filename);
    result = vfs_open(tempFilename,flags,0,&retvnode);
    if (result) {
        *ret = result;
        return -1;
    }
    
    int fd;
    
    result = create_file_and_add_to_table(curproc->p_ft, retvnode, flags, &fd);
    if (result) {
        *ret = result;
        return -1;
    }

    *ret = fd;

    return 0;
}
#endif

