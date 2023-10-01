#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"
#include "devices/shutdown.h"

#define USER_VADDR_BOTTOM ((void *) 0x08048000)


void syscall_init(void);

void syscall_exit(int status);
void close_remove_file(int fd);

struct file_sys {
    struct list_elem elem; // so i can go through the files for file_sys 
    struct file *file; //pointing to the actual file 
    int fd; // for file descriptor

};

struct lock sys_lock;

struct file* get_file_pointer(int fd);


#define STD_INPUT 0
#define STD_OUTPUT 1

#endif /* userprog/syscall.h */