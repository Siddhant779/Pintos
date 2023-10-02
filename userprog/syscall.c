#include <stdio.h>
#include <syscall-nr.h>

#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "process.h"
#include "exception.h"


bool sys_lock_init = false;

static void syscall_handler(struct intr_frame *);

void get_args_stack(int args_needed, struct intr_frame *f, int *arg_v);
//put all the syscall headers here 
void syscall_halt(void);
pid_t syscall_exec(const char* cmdline);
bool syscall_remove(const char* file_name);
bool syscall_create(const char* file_name, unsigned starting_size);
int syscall_open(const char *file_name);
int syscall_filesize(int fd);
void syscall_seek (int fd, unsigned new_position);
unsigned syscall_tell(int fd);
void syscall_close(int fd);
int syscall_read (int fd, void *buffer, unsigned size);
int syscall_write (int fd, const void *buffer, unsigned size);
int syscall_wait (pid_t pid);
void valid_ptr(void *v_ptr);

int syscall_wait (pid_t pid) {
  //need to update this for the syscall wait - going to be calling process_wait
  return process_wait(pid);
}

int syscall_write (int fd, const void *buffer, unsigned size) {
  if (size <= 0)
    {
      return size;
    }
    if (fd == STD_OUTPUT)
    {
      putbuf (buffer, size);
      return size;
    }
    
    // start writing to file
    //get lock here 
    lock_acquire(&sys_lock);
    struct file *file_pointer = get_file_pointer(fd);
    if (file_pointer == NULL)
    {
      //release lock here 
      lock_release(&sys_lock);
      return -1;
    }
    int size_write = file_write(file_pointer, buffer, size); 
    lock_release(&sys_lock);
    return size_write;
}

int syscall_read (int fd, void *buffer, unsigned size) {
  if (size <= 0) {
    return size;
  }
  if (fd == STD_INPUT)
  {
    uint8_t *buf_temp = (uint8_t *) buffer;
    for (unsigned i = 0;i < size; i++)
    {
      // retrieve pressed key from the input buffer
      buf_temp[i] = input_getc(); // from input.h
    }
    return size;
  }
  //put the lock here 
  lock_acquire(&sys_lock);
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    //release lock here 
    lock_release(&sys_lock);
    return -1;
  }
  int size_read = file_read(file_pointer, buffer, size); // from file.h
  //release the lock here 
  lock_release(&sys_lock);
  return size_read;

}

void close_file(int fd) {
  struct thread *t_curr = thread_current();
  struct list_elem *i;
  for (i = list_begin(&t_curr->file_list); i != list_end(&t_curr->file_list); i = list_next(i))
  {
    struct file_sys *file_pointer = list_entry(i, struct file_sys, elem); //Element itself, contained within file_sys, is what points to the prev and next elem (what makes file_sys actually a list)
    if (file_pointer->fd == fd) {
      file_close(file_pointer->file); //file is file pointer made by sid
      list_remove(&file_pointer->elem); //List remove is a function they made. Remove file by passing list elem
    }
  }

  return; // nothing was found 
}

struct file* get_file_pointer(int fd) {
  struct thread *t_curr = thread_current();
  struct list_elem *i;
  for (i = list_begin(&t_curr->file_list); i != list_end(&t_curr->file_list); i = list_next(i))
  {
    struct file_sys *file_pointer = list_entry(i, struct file_sys, elem);
    if (file_pointer->fd == fd) {
        return file_pointer->file;
      }
  }

  return NULL; // nothing was found 
}

bool
syscall_create(const char* file_name, unsigned starting_size)
{
  lock_acquire(&sys_lock);
  bool create = filesys_create(file_name, starting_size);
  lock_release(&sys_lock);
  return create;
}

bool
syscall_remove(const char* file_name) {
  lock_acquire(&sys_lock);
  bool remove = filesys_remove(file_name); 
  lock_release(&sys_lock);
  return remove;
}

int
syscall_open(const char *file_name) {
    //need to do locks here - global syslock here
  lock_acquire(&sys_lock);
  struct file *file_pointer = filesys_open(file_name); // from filesys.h
  if (file_pointer == NULL)
  {
    return -1;
  }
  //Add file to list
  struct file_sys *file_sys_pointer = malloc(sizeof(struct file_sys));
  if (file_sys_pointer == NULL)
  {
    lock_release(&sys_lock);
    return -1;
  }
  file_sys_pointer->file = file_pointer;
  file_sys_pointer->fd = thread_current()->fd;
  thread_current()->fd = thread_current()->fd + 1;
  int fd = file_sys_pointer->fd;
  list_push_back(&thread_current()->file_list, &file_sys_pointer->elem);
  lock_release(&sys_lock);
  return fd;
}

int
syscall_filesize(int fd)
{
    //need to do locks here - global syslock here 
  lock_acquire(&sys_lock);
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    lock_release(&sys_lock);
    return -1;
  }
  int file_size = file_length(file_pointer); 
  lock_release(&sys_lock);
  return file_size;
}

void
syscall_seek (int fd, unsigned new_position)
{
  lock_acquire(&sys_lock);
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    lock_release(&sys_lock);
    return;
  }
  file_seek(file_pointer, new_position);
  lock_release(&sys_lock);
}

unsigned
syscall_tell(int fd)
{
  lock_acquire(&sys_lock);
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    lock_release(&sys_lock);
    return -1;
  }
  off_t offset = file_tell(file_pointer); 
  lock_release(&sys_lock);
  return offset;
}

void
syscall_close(int fd)
{
    lock_acquire(&sys_lock);
    close_file(fd);
    lock_release(&sys_lock);
}

pid_t syscall_exec(const char* cmdline) {
    //Check if file exists (fix exec missing)
    struct file* file = filesys_open(cmdline);
    if(file == NULL){
      return -1;
    }

    //Execute file
    pid_t pid = process_execute(cmdline);

    //semaphore stuff here not sure we should discuss this 
    //gets the thread and executes it 
    return pid;

}
void valid_ptr(void *pointer) {
    if (pointer < USER_VADDR_BOTTOM || !is_user_vaddr(pointer))
    {
      // virtual memory address is not reserved for us (out of bound)
      syscall_exit(-1);
    }
}
void syscall_exit(int status) {
    struct thread *t_curr = thread_current();
    //DO I NEED TO SEE THE CONDITION OF THE PARENT
    // if(thread_exists(t_curr->tid)) {
    //     if(status < 0) {
    //         status = -1;
    //     }
    //     t_curr->status = status;
    // }
    printf("%s: exit(%d)\n", t_curr->name, status);
    thread_exit();
}  

void syscall_halt() {
    shutdown_power_off();
}
void get_args_stack(int args_needed, struct intr_frame *f, int *arg_v) {
    int *ptr;
    for (int i = 0; i < args_needed; i++)
    {
        ptr = (int *) f->esp + i + 1;
        valid_ptr((void *) ptr);
        arg_v[i] = *ptr;
    }
}
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

void
syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
    /* Remove these when implementing syscalls */
    if(sys_lock_init == false) {
        lock_init(&sys_lock);
        sys_lock_init = true;
    }
    //Get value of signal
    // int *stackPointer = f->esp;
    void *stackPhysicalPointer = pagedir_get_page(thread_current()->pagedir, (const void *) f->esp);
    if(stackPhysicalPointer == NULL) {
      return syscall_exit(-1);
    }
    int stackPointer = (int)stackPhysicalPointer;
    if(stackPhysicalPointer == NULL || stackPhysicalPointer > PHYS_BASE) {
        // there is something wrong with the stackpointer - in the kernel virtual address space right now 
    }
    else {
        //dereference pointer
        // this is where we call the get_user and put_user
        int test = get_user(stackPointer); // read at stack pointer
        if(test == -1 ) { //returns -1 if you can't write 
            page_fault_handler(f);
        }
        else {
            if(put_user(stackPointer, test) == false) { //write that back to stack pointer (make sure you can write back to stack pointer) Not destroying anoything because you are writing what you read
                page_fault_handler(f); // does page_fault get called automatically, and if not, how to include it?
            }
        }
        
        //no error

    }

    uint32_t signal = * (int *) stackPointer;
    int args_v[3];
        
    if(signal == SYS_CLOSE) {
        
      get_args_stack(1,f,&args_v[0]);
      syscall_close(args_v[0]);

    } else if(signal == SYS_CREATE) {
        get_args_stack(2,f, &args_v[0]);
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr; // gets the actual address 
        f->eax = syscall_create((const char *)args_v[0], (unsigned)args_v[1]);
        
    } else if(signal == SYS_EXEC) {

        get_args_stack(1,f, &args_v[0]);
        //DO WE NEED TO CHECK IF THE COMMAND IS VALID 
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr;  // gets the actual address 
        f->eax = syscall_exec((const char *)args_v[0]);
        
    } else if(signal == SYS_EXIT) {

        get_args_stack(1,f, &args_v[0]);
        syscall_exit(args_v[0]);

    } else if(signal == SYS_FILESIZE) {

      get_args_stack(1, f, &args_v[0]); 
      f->eax = syscall_filesize(args_v[0]);  // obtain file size
        
    } else if(signal == SYS_HALT) {
        syscall_halt();
        
    } else if(signal == SYS_OPEN) {
        get_args_stack(1, f, &args_v[0]);
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr;  // gets the actual address 
        f->eax = syscall_open((const char *)args_v[0]);  // remove this file
        
    } else if(signal == SYS_READ) {
        get_args_stack(3, f, &args_v[0]);
        //making sure the buffer is in good memory 
        unsigned length_size = (unsigned) args_v[2];
        char *buffer_temp = (char *) args_v[1];
        for(unsigned i = 0; i < length_size; i++) {
          valid_ptr((void *) buffer_temp);
          buffer_temp++;
        }
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[1]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[1] = (int)ptr;
        f->eax = syscall_read(args_v[0], (void *) args_v[1], length_size);

    } else if(signal == SYS_REMOVE) {
        get_args_stack(1, f, &args_v[0]);
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr;  // gets the actual address 
        f->eax = syscall_remove((const char *)args_v[0]);  // remove this file

    } else if(signal == SYS_SEEK) {
        // file_seek
      get_args_stack(2, f, &args_v[0]);
      syscall_seek(args_v[0], (unsigned)args_v[1]);
    } else if(signal == SYS_TELL) {
      get_args_stack(1, f, &args_v[0]);

      f->eax = syscall_tell(args_v[0]);

    } else if(signal == SYS_WAIT) {
      get_args_stack(1, f, &args_v[0]);
      f->eax = syscall_wait(args_v[0]);

    } else if(signal == SYS_WRITE) {
        // Write signal
        get_args_stack(3, f, &args_v[0]);
        //making sure the buffer is in good memory 
        unsigned length_size = (unsigned) args_v[2];
        char *buffer_temp = (char *) args_v[1];
        for(unsigned i = 0; i < length_size; i++) {
          valid_ptr((void *) buffer_temp);
          buffer_temp++;
        }
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[1]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[1] = (int)ptr;
        f->eax = syscall_write(args_v[0], (const void *) args_v[1], (unsigned) args_v[2]);
    }

    // thread_exit();
}