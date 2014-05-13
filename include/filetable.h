#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include <vnode.h>
#include <limits.h>
#include <bitmap.h>
#include <types.h>
#include <synch.h>
#include "opt-A2.h"

#if OPT_A2

struct File{
	struct vnode * vn;
	int flags; // readable/writable
	unsigned int fd;
	volatile off_t offset;
    struct lock* rw_lock;
};

struct FileTable{
	struct File * files[OPEN_MAX];
	unsigned int num_files;
	struct bitmap * bm;
};

struct FileTable * create_filetable(void);

int destroy_filetable(struct FileTable * ft);
int duplicate_filetable(struct FileTable * src, struct FileTable * dest);
int create_file_and_add_to_table(struct FileTable * ft, struct vnode *vn, int flags, int * fd);
int file_exists_in_table(struct FileTable * ft, unsigned int fd);
int close_file_and_remove_from_table(struct FileTable * ft, unsigned int fd);

int get_file_by_id(struct FileTable * ft, unsigned int fd, struct File * ret);

/* since we close file we destroy the file structure */

//int destroy_file(struct File * f);

#endif /* OPT_A2 */

#endif /* _FILETABLE_H_ */
