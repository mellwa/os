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
#include <kern/fcntl.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include "opt-A3.h"

#if OPT_A2
int execv(const char* progname, char ** argv, int32_t* retval){
    struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
    
    if(vm_invalidaddress((vaddr_t)progname) || vm_invalidaddress((vaddr_t)argv)){
        *retval = EFAULT;
        return -1;
    }
    if(strlen(progname) == 0){
        *retval = EINVAL;
        return -1;
    }
    if(!strcmp(progname,"con:")){
        *retval = ENODEV;
        return -1;
    }
    int l = strlen(progname) + 1; 
    int prog_len = (sizeof(char))*l;
    size_t actual_len;
    char prog[l];
    *retval = copyinstr((const_userptr_t)progname,prog,prog_len,&actual_len);
    if(*retval){
        return -1;
    }

    unsigned int argc = 0;
    //calculate the number of args
    while(argv[argc] !=  NULL){
        argc++;
    }
    
    //check if the addresses of args are valid
        if(vm_invalidaddress((vaddr_t)argv[0]) || vm_invalidaddress((vaddr_t)argv)){
            *retval = EFAULT;
            return -1;
        }
    unsigned int z;
        for(z=1;argv[z] != NULL;z++){
            if(vm_invalidaddress((vaddr_t)argv[z])){
                *retval = EFAULT;
                return -1;
            }
        }
    
    
    char* temp_argv[argc+1];
    int argv_len;
    for(z = 0; z < argc; z++){
        argv_len = (strlen(argv[z])+1)*(sizeof(char));
        temp_argv[z] = kmalloc(argv_len);
        if(temp_argv[z] == NULL){
            *retval = ENOMEM;
            return -1;
        }
        *retval = copyinstr((const_userptr_t)argv[z],temp_argv[z],argv_len,&actual_len);
        if(*retval){
            return -1;
        }
    }
    
    if(curproc_getas() != NULL){
		as_deactivate();
		as = curproc_setas(NULL);
		as_destroy(as);
    }
    
	/* Open the file. */
	result = vfs_open((char *)prog, O_RDONLY, 0, &v);
	if (result) {
        *retval = result;
		return -1;
	}


	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);
    
	/* Create a new address space. */
	#if OPT_A3
	as = as_create((char *)progname);
	#else
	as = as_create();
	#endif
	if (as ==NULL) {
		vfs_close(v);
        *retval = ENOMEM;
		return -1;
	}
    
    
	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();
    
    
	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
        *retval = result;
		return -1;
	}
    
	/* Done with the file now. */
	vfs_close(v);
	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
        *retval = result;
		return -1;
	}
    
    
    


    //add stuff
    stackptr = stackptr - (argc + 1)*sizeof(vaddr_t);
    
    
    char **buf = (char **)stackptr;
    unsigned long i;
    unsigned int len;
    for(i=0; i<argc; i++){
        //kprintf("%lu argv: %s\n",i,temp_argv[i]);
        len = (strlen(temp_argv[i])+1)*(sizeof(char));
        //kprintf("string length is: %u\n",len);
        stackptr = stackptr - len;
        
        buf[i] = (char*)stackptr;
        result = copyout((void *)temp_argv[i],(userptr_t)buf[i],len);
        if(result){
            *retval = result;
            return -1;
        }
    }
    unsigned int remainder = stackptr % 8;
	if(remainder != 0){
		stackptr = stackptr - remainder;
	}
    
    for(z=0;z<argc;z++){
        if(temp_argv[z] != NULL){
            kfree(temp_argv[z]);
        }
    }
    
	/* Warp to user mode. */
	enter_new_process(argc, (userptr_t)buf /*userspace addr of argv*/,
                      stackptr, entrypoint);
	
    
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
    *retval = EINVAL;
	return -1;
}
#endif

