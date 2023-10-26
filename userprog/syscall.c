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
/*
static void
check_user (const uint8_t *uaddr) {
  // check uaddr range or segfaults
  if(get_user (uaddr) == -1)
    fail_invalid_access();
}

static int32_t
get_user (const uint8_t *uaddr) {
  // check that a user pointer `uaddr` points below PHYS_BASE
  if (! ((void*)uaddr < PHYS_BASE)) {
    return -1;
  }

  // as suggested in the reference manual, see (3.1.5)
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

static int
memread_user (void *src, void *dst, size_t bytes)
{
  int32_t value;
  size_t i;
  for(i=0; i<bytes; i++) {
    value = get_user(src + i);
    if(value == -1) // segfault or invalid memory access
      fail_invalid_access();

    *(char*)(dst + i) = value & 0xff;
  }
  return (int)bytes;
}

 memread_user(f->esp + 4, &fd, sizeof(fd));
      memread_user(f->esp + 8, &buffer, sizeof(buffer));
      memread_user(f->esp + 12, &size, sizeof(size));

      return_code = sys_write(fd, buffer, size);
      f->eax = (uint32_t) return_code;
      break;


int sys_write(int fd, const void *buffer, unsigned size) {
  // memory validation : [buffer+0, buffer+size) should be all valid
  check_user((const uint8_t*) buffer);
  check_user((const uint8_t*) buffer + size - 1);
*/


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
int syscall_read (int fd, void *buffer, unsigned size, struct intr_frame *f);
int syscall_write (int fd, const void *buffer, unsigned size, struct intr_frame *f);
int syscall_wait (pid_t pid);
// void valid_ptr(void *v_ptr);
bool syscall_create(const char* file_name, unsigned starting_size);

// this code get_user was provided by the pintos documentation
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
 
// this code put_user was provided by the pintos documentation
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

static void
check_addy(const uint8_t *addr, struct intr_frame *f, bool write) {
  valid_ptr((void *) addr);
  int test = get_user(addr);
  if(test == -1) {
    page_fault_handler(f);
  }
  else {
    if(write) {
      if(put_user(addr, test) == false) {
        page_fault_handler(f);
      }
    }
  }
}

int syscall_wait (pid_t pid) {
  //need to update this for the syscall wait - going to be calling process_wait
  return process_wait(pid);
}

//returns the size 
int syscall_write (int fd, const void *buffer, unsigned size, struct intr_frame *f) {
  //check_addy((const uint8_t *) buffer, f, true);
  //check_addy((const uint8_t *) buffer + size - 1, f, true);
  if (size <= 0)
    {
      //if the size of what we are writing to hte file is under 1 then return the value back and dont write 
      return size;
    }
    if (fd == STD_OUTPUT)
    {
      // if we are writing to the console then use the putbuf a function in console.c to write to the console
      putbuf (buffer, size);
      return size;
    }
    
    // start writing to file
    //get lock here 
    lock_acquire(&sys_lock);
    // calls get_file_pointer to search for the file given the file descriptor 
    struct file *file_pointer = get_file_pointer(fd);
    //no file found return -1
    if (file_pointer == NULL)
    {
      //release lock here 
      lock_release(&sys_lock);
      return -1;
    }
    //get the size of how many bytes were written - called a function in file.c
    int size_write = file_write(file_pointer, buffer, size); 
    //release lock here
    lock_release(&sys_lock);
    return size_write;
}

int syscall_read (int fd, void *buffer, unsigned size, struct intr_frame *f) {
  //check_addy((const uint8_t *)buffer, f, false);
  //check_addy((const uint8_t *)buffer + size -1, f, false);
  if (size <= 0) {
    // can't read less than one character so just return the size back 
    return size;
  }
  if (fd == STD_INPUT)
  {
    // if its console input then use input_getc() a fucntion from console.c to read from the console a character a time 
    uint8_t *buf_temp = (uint8_t *) buffer;
    for (unsigned i = 0;i < size; i++)
    {
      buf_temp[i] = input_getc();
    }
    //return the size 
    return size;
  }
  //put the lock here 
  lock_acquire(&sys_lock);
  //get the file pointer for specific file descriptor ; if null return -1 - doesnt exist 
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    //release lock here 
    lock_release(&sys_lock);
    return -1;
  }
  //reads the file - a function in file.c 
  int size_read = file_read(file_pointer, buffer, size);
  //release the lock here 
  lock_release(&sys_lock);
  return size_read;

}

// closes the file pased on the input of file descriptor 
void close_file(int fd) {
  struct thread *t_curr = thread_current();
  struct list_elem *i;
  //goes through the file list for that specific thread and finds the file pointer fd and closes the file 
  for (i = list_begin(&t_curr->file_list); i != list_end(&t_curr->file_list); i = list_next(i))
  {
    struct file_sys *file_pointer = list_entry(i, struct file_sys, elem); //Element itself, contained within file_sys, is what points to the prev and next elem (what makes file_sys actually a list)
    if (file_pointer->fd == fd) {
      //found the fd close the file descriptor for that specific fd 
      file_close(file_pointer->file);
      list_remove(&file_pointer->elem); //List remove is a function they made. Remove file by passing list elem
    }
  }

  return; // nothing was found 
}

//returns the file pointer based on the specific fd - other functions call this function to return a file pointer 
struct file* get_file_pointer(int fd) {
  struct thread *t_curr = thread_current();
  struct list_elem *i;
  //uses the for loop from file.h to find the specific file based on the fd 
  for (i = list_begin(&t_curr->file_list); i != list_end(&t_curr->file_list); i = list_next(i))
  {
    struct file_sys *file_pointer = list_entry(i, struct file_sys, elem);
    if (file_pointer->fd == fd) {
        return file_pointer->file; // return file based on input fd
      }
  }

  return NULL; // nothing was found 
}

bool
syscall_create(const char* file_name, unsigned starting_size)
{
  //calls filesys create function in filesys.c for creating a new file
  //lock for when accessing files 
  lock_acquire(&sys_lock);
  bool create = filesys_create(file_name, starting_size);
  lock_release(&sys_lock);
  return create;
}

bool
syscall_remove(const char* file_name) {
  //calls filesys remove function in filesys.c for removing a file
  //lock for when accessing files 
  lock_acquire(&sys_lock);
  bool remove = filesys_remove(file_name); 
  lock_release(&sys_lock);
  return remove;
}

int
syscall_open(const char *file_name) {
  //lock for when accessing files 
  lock_acquire(&sys_lock);
  // opens a file based on the name - filesys.c function 
  struct file *file_pointer = filesys_open(file_name);
  if (file_pointer == NULL)
  {
    lock_release(&sys_lock);
    return -1;
  }
  //Add file to list that exists for each thread 
  struct file_sys *file_sys_pointer = malloc(sizeof(struct file_sys));
  if (file_sys_pointer == NULL)
  {
    
    lock_release(&sys_lock);
    return -1;
  }
  //populating the struct file_sys - with file pointer and fd 
  file_sys_pointer->file = file_pointer;
  file_sys_pointer->fd = thread_current()->fd;
  //incrementing the global fd for the thread 
  thread_current()->fd = thread_current()->fd + 1;
  //adding to the list, removing lock, and returning fd
  int fd = file_sys_pointer->fd;
  list_push_back(&thread_current()->file_list, &file_sys_pointer->elem);
  lock_release(&sys_lock);
  return fd;
}

int
syscall_filesize(int fd)
{
  //lock for when accessing files 
  lock_acquire(&sys_lock);
  //gets the file pointer based on fd
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    lock_release(&sys_lock);
    return -1;
  }
  //uses file.c function to get the file length based on the pointer 
  int file_size = file_length(file_pointer); 
  lock_release(&sys_lock);
  return file_size;
}

void
syscall_seek (int fd, unsigned new_position)
{
  //lock for when accessing files 
  lock_acquire(&sys_lock);
  //gets the file pointer based on fd
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    lock_release(&sys_lock);
    return;
  }
  //uses file.c function to set the position in file
  file_seek(file_pointer, new_position);
  lock_release(&sys_lock);
}

unsigned
syscall_tell(int fd)
{
  //lock for when accessing files 
  lock_acquire(&sys_lock);
  //gets the file pointer based on fd
  struct file *file_pointer = get_file_pointer(fd);
  if (file_pointer == NULL)
  {
    lock_release(&sys_lock);
    return -1;
  }
  //uses file.c function to get the position in file
  off_t offset = file_tell(file_pointer); 
  lock_release(&sys_lock);
  return offset;
}

void
syscall_close(int fd)
{
    //closes the file - needs locks for that 
    lock_acquire(&sys_lock);
    close_file(fd);
    lock_release(&sys_lock);
}

pid_t syscall_exec(const char* cmdline) {
  //calls process_execute for syscall_exe
  pid_t pid = process_execute(cmdline);

  return pid;

}
//makes sure that the virtual address is a valid user address
void valid_ptr(void *pointer) {
   if (!is_user_vaddr(pointer))
    {
      // virtual memory address is not reserved for us (out of bound)
      syscall_exit(-1);
    }
    // if (pointer < USER_VADDR_BOTTOM || !is_user_vaddr(pointer))
    // {
    //   // virtual memory address is not reserved for us (out of bound)
    //   syscall_exit(-1);
    // }
}
void syscall_exit(int status) {
    struct thread *t_curr = thread_current();
    // sents the thread exit status to status and prints out the exit statement for the thread
    t_curr->exit_status = status;
    printf("%s: exit(%d)\n", t_curr->name, status);
    thread_exit();
}  

void syscall_halt() {
  //syscall halt 
    shutdown_power_off();
}

void get_args_stack(int args_needed, struct intr_frame *f, int *arg_v) {
    // first increments the pointer and gets the value - this is because we got the signal at the very beginning - opposite of what pop usually does 
    int *ptr;
    for (int i = 0; i < args_needed; i++)
    {
        ptr = (int *) f->esp + i + 1;
        valid_ptr((void *) ptr);
        // int test = get_user((const uint8_t *) ptr); // read at stack pointer
        // if(test == -1 ) { //returns -1 if you can't write 
        //   //page fault if its -1
        //     page_fault_handler(f);
        // }
        //check_addy((const uint8_t *)ptr, f, false);
        arg_v[i] = *ptr;
    }
}

void
syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    if(sys_lock_init == false) {
        lock_init(&sys_lock);
        sys_lock_init = true;
    }
}

static int
memory_copy (void *src, void *dst, size_t bytes)
{
  int32_t value;
  size_t i;
  for(i=0; i<bytes; i++) {
    valid_ptr((void *) (src+i));
    value = get_user(src + i);
    if(value == -1) // segfault or invalid memory access
      page_fault_handler(-1);

    *(char*)(dst + i) = value & 0xff;
  }
  return (int)bytes;
}


static void
syscall_handler(struct intr_frame *f UNUSED)
{
  //if its the first time initializing the syslock then do a lock_init and set sys_lock_init to true
    //Get value of signal
    void *stackPhysicalPointer = pagedir_get_page(thread_current()->pagedir, (const void *) f->esp);
    //if the stack pointer is null or its above PHYS_Base then its not a good pointer 
    if(stackPhysicalPointer == NULL) {
      return syscall_exit(-1);
    }
    int stackPointer = (int)stackPhysicalPointer;
    if(stackPhysicalPointer == NULL || stackPhysicalPointer > PHYS_BASE) {
    }
    else {
        //dereference pointer
        // this is where we call the get_user and put_user
        //check_addy((const uint8_t *) stackPointer, f, true);
        int test = get_user(stackPointer); // read at stack pointer
        if(test == -1 ) { //returns -1 if you can't write 
          //page fault if its -1
            page_fault_handler(f);
        }
        else {
            if(put_user(stackPointer, test) == false) { //write that back to stack pointer (make sure you can write back to stack pointer) Not destroying anoything because you are writing what you read
                //page fault if its false
                page_fault_handler(f); 
            }
        }
        
    }

    uint32_t signal = * (int *) stackPointer;
    int args_v[3];
    //if statements for each signal
    // each function gets however many arguments it needs from the get_args_stack function and then calls its respective syscall function
    if(signal == SYS_CLOSE) {
      
      get_args_stack(1,f,&args_v[0]);
      syscall_close(args_v[0]);

    } else if(signal == SYS_CREATE) {
        get_args_stack(2,f, &args_v[0]);
        // get the physical address for the pointer to the file
        check_addy((const uint8_t *) args_v[0], f, true);
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr; // gets the actual address 
        //syscall create returns a bool so store that in eax
        f->eax = syscall_create((const char *)args_v[0], (unsigned)args_v[1]);
        
    } else if(signal == SYS_EXEC) {

        get_args_stack(1,f, &args_v[0]);
         // get the physical address for the pointer to the char *cmd_line
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr;  // gets the actual address 
        //store the pid in eax syscall_exec returns a pid
        f->eax = syscall_exec((const char *)args_v[0]);
        
    } else if(signal == SYS_EXIT) {

        get_args_stack(1,f, &args_v[0]);
        syscall_exit(args_v[0]);

    } else if(signal == SYS_FILESIZE) {

      get_args_stack(1, f, &args_v[0]); 
      //store the size of the file in eax
      f->eax = syscall_filesize(args_v[0]);  // obtain file size
        
    } else if(signal == SYS_HALT) {
        syscall_halt();
        
    } else if(signal == SYS_OPEN) {
        get_args_stack(1, f, &args_v[0]);
        //gets the physical address for the pointer to the file 
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr;  // gets the actual address 
        //stores the fd in eax that was returned by syscall_open
        f->eax = syscall_open((const char *)args_v[0]);  // remove this file
        
    } else if(signal == SYS_READ) {
      int fd, return_code;
      void *buffer;
      unsigned size;

      memory_copy(f->esp + 4, &fd, sizeof(fd));
      memory_copy(f->esp + 8, &buffer, sizeof(buffer));
      memory_copy(f->esp + 12, &size, sizeof(size));
      for(int i = 0; i < size; i++) {
        check_addy((const uint8_t *)buffer+i,f,true);
      }

      return_code = syscall_read(fd, buffer, size,f);
      f->eax = (uint32_t) return_code;

        // get_args_stack(3, f, &args_v[0]);
        // //making sure the buffer is in good memory 
        // unsigned length_size = (unsigned) args_v[2];
        // char *buffer_temp = (char *) args_v[1];
        // for(unsigned i = 0; i < length_size; i++) {
        //   valid_ptr((void *) buffer_temp+i);
        //   int test = get_user((void *) (buffer_temp+i)); // read at stack pointer
        //   if(test == -1 ) { //returns -1 if you can't write 
        //     //page fault if its -1
        //     page_fault_handler(f);
        //   }
        //   else {
        //     if(put_user(((void *) buffer_temp+i), test) == false) { //write that back to stack pointer (make sure you can write back to stack pointer) Not destroying anoything because you are writing what you read
        //         //page fault if its false
        //         page_fault_handler(f); 
        //     }
        // }
        //   //check_addy((const uint8_t *) buffer_temp + i, f, false);
        //   // buffer_temp++;
        // }
        // //gets the physical address for the buffer pointer
        // void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[1]);
        // if(ptr == NULL) {
        //   return syscall_exit(-1);
        // }
        // args_v[1] = (int)ptr;
        // //stores the number of bytes that were read into the eax 
        // f->eax = syscall_read(args_v[0], (void *) args_v[1], length_size, f);

    } else if(signal == SYS_REMOVE) {
        get_args_stack(1, f, &args_v[0]);
        //gets the physical address for the file pointer 
        void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[0]);
        if(ptr == NULL) {
          return syscall_exit(-1);
        }
        args_v[0] = (int)ptr;  // gets the actual address 
        //syscall remove either returns true or false depending on if it removed it - stores that in eax
        f->eax = syscall_remove((const char *)args_v[0]);  // remove this file

    } else if(signal == SYS_SEEK) {
        // file_seek
      get_args_stack(2, f, &args_v[0]);
      syscall_seek(args_v[0], (unsigned)args_v[1]);
    } else if(signal == SYS_TELL) {
      get_args_stack(1, f, &args_v[0]);
      //returns the position of the next byte to be read in eax 
      f->eax = syscall_tell(args_v[0]);

    } else if(signal == SYS_WAIT) {
      get_args_stack(1, f, &args_v[0]);
      //returns the wait signal exit status to eax
      f->eax = syscall_wait(args_v[0]);

    } else if(signal == SYS_WRITE) {

      int fd, return_code;
      const void *buffer;
      unsigned size;

      memory_copy(f->esp + 4, &fd, sizeof(fd));
      memory_copy(f->esp + 8, &buffer, sizeof(buffer));
      memory_copy(f->esp + 12, &size, sizeof(size));
      for(int i = 0; i < size; i++) {
        check_addy((const uint8_t *)buffer+i,f,false);
      }

      return_code = syscall_write(fd, buffer, size, f);
      f->eax = (uint32_t) return_code;


      //   // Write signal
      //   get_args_stack(3, f, &args_v[0]);
      //   //making sure the buffer is in good memory 
      //   unsigned length_size = (unsigned) args_v[2];
      //   char *buffer_temp = (char *) args_v[1];
      //   for(size_t i = 0; i < length_size; i++) {
      //     valid_ptr((void *) buffer_temp+i);
      //     int test = get_user((void *) (buffer_temp+i)); // read at stack pointer
      //     if(test == -1 ) { //returns -1 if you can't write 
      //       //page fault if its -1
      //       page_fault_handler(f);
      //     }
      //   //check_addy((const uint8_t *) buffer_temp + i, f, true);
      //   // buffer_temp++;
      // }
      //   //getting the physical address of the buffer 
      //   void *ptr = pagedir_get_page(thread_current()->pagedir, (const void *) args_v[1]);
      //   if(ptr == NULL) {
      //     return syscall_exit(-1);
      //   }
      //   args_v[1] = (int)ptr;
      //   //returns the number of bytes written into eax 
      //   f->eax = syscall_write(args_v[0], (const void *) args_v[1], (unsigned) args_v[2], f);
    }
}