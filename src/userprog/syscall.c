#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void is_bad_args(int *p, int argc){
  bool bad = false;
  struct thread *t  = thread_current();
  if(argc == 0){
    void *esp = p;
    if(esp >= PHYS_BASE)
      bad = true;
    if(!bad)
      if(!pagedir_get_page(t->pagedir, esp))
	bad = true;
  }
  else if(argc == 1){
    void *arg1 = p + 1;
    if(arg1 >= PHYS_BASE)
      bad = true;
    if(!bad)
      if(!pagedir_get_page(t->pagedir, arg1))
	bad = true;
  }
  else if(argc == 2){
    void *arg1 = p + 1;
    void *arg2 = p + 2;
    if(arg1 >= PHYS_BASE || arg2 >= PHYS_BASE)
      bad = true;
    if(!bad)
      if(!pagedir_get_page(t->pagedir, arg1) || \
	 !pagedir_get_page(t->pagedir, arg2))
	bad = true;
  }
  else{
    void *arg1 = p + 1;
    void *arg2 = p + 2;
    void *arg3 = p + 3;
    if(arg1 >= PHYS_BASE || arg2 >= PHYS_BASE || arg3 >= PHYS_BASE)
      bad = true;
    if(!bad)
      if(!pagedir_get_page(t->pagedir, arg1) || \
	 !pagedir_get_page(t->pagedir, arg2) || \
	 !pagedir_get_page(t->pagedir, arg3))
	bad = true;
    }
  
  if(bad){
    //printf("%s: exit(%d)\n", t->name, -1);
    t->exit_status = -1;
    thread_exit();
  }
    
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    int *p = f->esp;
    struct thread *t = thread_current();
    is_bad_args(p, 0);
    switch(*p){
    case SYS_HALT:
      break;
    case SYS_EXIT:
      is_bad_args(p, 1);
      int *status = p + 1;
      //remove because maybe better to print inside process_exit()
      //printf("%s: exit(%d)\n", t->name, *status);
      f->eax = *status;
      t->exit_status = *status;
      thread_exit();
      break;
    case SYS_WRITE:
      is_bad_args(p, 3);
      int *fd = p + 1;
      const char **buffer = (const char **)(p + 2);
      unsigned *size = (unsigned *)(p + 3);
      if(*fd == 0)
	;//writing to stdin
      else if(*fd == 1)
	putbuf(*buffer, *size);
      else {
	;//writing to file
      }
      break;
    case SYS_CREATE:
      ;
      const char** file = (const char**)(p+1);
      //printf("\nBEFORE filesys_create, filename: %s\n");
      unsigned initial_size = *(unsigned*)(p+2);
      is_bad_args(p, 2);
      bool success = filesys_create(*file, initial_size);
      //printf("\nSUCCESS = %d\nfilename: %s\nsize: %d\n", success, *file, initial_size);
      /*
      if (!(*file)) {
	printf("\nFILENAME == NULL\n");
	success = false;
      }
      */
      if (!success) {
	//enum intr_level old_level = intr_disable();
	//t->exit_status = 0;
	f->eax = success;
	//thread_exit();
	//intr_set_level(old_level);
      }
      else {
      f->eax = success;
      }
      break;
    }
}
