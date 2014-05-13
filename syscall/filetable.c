#include <types.h>
#include <filetable.h>
#include <lib.h>
#include <kern/errno.h>
#include "opt-A2.h"
#include <vfs.h>
#include <syscall.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <vnode.h>

#if OPT_A2

//operations for FileTable

struct FileTable * create_filetable(void) {
    //int err;
    //int32_t ret;
	struct FileTable * ft = kmalloc(sizeof(struct FileTable));
    if (ft == NULL) {
        return NULL;
    }
	ft->num_files = 0; // stdin, stdout, stderr
	ft->bm = bitmap_create(OPEN_MAX);
    if (ft->bm == NULL) {
	    kfree(ft);
        return NULL;
    }
	
	// mark stdin/out/err
    //bitmap_mark(ft->bm, 0);
	//bitmap_mark(ft->bm, 1);
	//bitmap_mark(ft->bm, 2);
    return ft;
}

int duplicate_filetable(struct FileTable * src, struct FileTable * dest) {
	KASSERT(dest != NULL);
	dest->num_files = src->num_files;
	dest->bm = bitmap_create(OPEN_MAX);
	if (dest->bm == NULL) {
		kfree(dest);
		return -1;
	}
	
	for(int i = 0; i < OPEN_MAX; i++) {
		if(bitmap_isset(src->bm, i)) {
			bitmap_mark(dest->bm, i);
			dest->files[i] = src->files[i];
            VOP_INCOPEN(dest->files[i]->vn);//increase the num of open file
		} else {
			dest->files[i] = NULL;
		}
	}
	
	return 0;
}

int destroy_filetable(struct FileTable * ft) {
	//check if there is any other files open besides stdin/out/err
    //KASSERT(ft->num_files == 3);
    
    //remove stdin/out/err from file table
    for (int i = 0; i < 3; i ++) {
        if(bitmap_isset(ft->bm, i)) {
            ft->files[i] = NULL;
            ft->num_files --;
            bitmap_unmark(ft->bm, i);	
		}
    }
	
//	for(int i = 3; i < OPEN_MAX; i++) {
//		if(bitmap_isset(ft->bm, i)) {
//			kfree(ft->files[i]);	
//		}
//	}
	
	//bitmap_destroy(ft->bm);
    //ft->bm = NULL;
    ft = NULL;
	//kfree(ft);
	return 0;
}

// returns file handle
int create_file_and_add_to_table(struct FileTable * ft, struct vnode *vn, int flags, int * fd) {
    struct File * f = kmalloc(sizeof(struct File));
    if (f == NULL) {
        return -1;
    }
    f->vn = vn;
    f->flags = flags;
    f->offset = 0;
    f->rw_lock = lock_create("rw_lock");
    if(f->rw_lock == NULL){
        kfree(f);
        return -1;
    }
    for (int i = 0; i < OPEN_MAX; i++) {
        if (!bitmap_isset(ft->bm, i)) {
            f->fd = i;
            bitmap_mark(ft->bm, i);
            ft->files[i] = f;
            ft->num_files ++;
            *fd = i;
            return 0;
        }
    }
    
    return EMFILE;  /* process's file table is full */
}

int file_exists_in_table(struct FileTable * ft, unsigned int fd) {
    
	if(fd >= OPEN_MAX) {
		return EBADF; /* invalid file handle */
	}
	
	return bitmap_isset(ft->bm, fd);
}


/* the fd is a valid file handle */
int close_file_and_remove_from_table(struct FileTable * ft, unsigned int fd) {	
    if (!bitmap_isset(ft->bm, fd)) {
        return -1; /* the file is not open */
    }
    struct File *f = ft->files[fd];
    vfs_close(f->vn); /* close vnode; wouldn't fail */
    f->vn = NULL;
    lock_destroy(f->rw_lock);
    kfree(f);
    
    bitmap_unmark(ft->bm, fd);
	ft->files[fd] = NULL;
	ft->num_files --;
	
	return 0;
}


//operations for File

int get_file_by_id(struct FileTable * ft, unsigned int fd, struct File * ret) {
	if(fd >= OPEN_MAX) {
		return EBADF; /* invalid file handle */
	}
    
    /* check if the file is in filetable */
    KASSERT(bitmap_isset(ft->bm, fd));
        ret = ft->files[fd];
        return 0;
}

#endif /* OPT_A2 */
