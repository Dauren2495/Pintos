#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    int *p = f->esp;
    struct thread *t = thread_current();
    if(pagedir_get_page(t->pagedir, p ) != (void*)NULL)
    {
	switch(*p){
        case SYS_HALT:
            //halt(f);
            break;
        case SYS_EXIT:
	  //printf("in SYS_EXIT \n");
	  ;
	  int *status = p + 1;
            if(!pagedir_get_page(t->pagedir, status))
                thread_exit();
	    printf("%s: exit(%d)\n", t->name, *status);
	    f->eax = *status;
	    thread_exit();
            break;
        case SYS_WRITE:
	  //printf("in SYS_WRITE \n");
	  ;
	  int *fd = p + 1;
	  const char **buffer = (const char**)(p + 2);
	  unsigned *size = (unsigned *)(p + 3);
            if((!pagedir_get_page(t->pagedir, fd)) ||       \
               (!pagedir_get_page(t->pagedir, buffer)) ||   \
               (!pagedir_get_page(t->pagedir, size)))
	      printf("Invalid to SYS_Write\n");
            if(*fd == 0)
		;//writing to stdin
            else if(*fd == 1)
		putbuf(*buffer, *size);
            else {
		;//writing to file
	    }
	    break;
	    /*
	case SYS_CREATE:
	  ;
	  const char* file = (const char*)(p+1);
	  unsigned* pinitial_size = (unsigned*)(p+2);
	  bool success = false;
	  if ( (!pagedir_get_page(t->pagedir, file)) || \
	       (!pagedir_get_page(t->pagedir, pinitial_size))) {
	    printf("Invalid argument supplied to SYS_CREATE\n");
	    f->eax = success;
	    thread_exit();
	  }
	  else {
	    success = filesys_create(file, *pinitial_size);
	    f->eax = success;
	  }
	  break;*/
        }
        //printf ("system call number is %d!\n", ((void*)f->esp + 4));
    }
    else
      printf("invalid pointer\n");

  //printf ("system call!\n");
  //thread_exit ();
}
