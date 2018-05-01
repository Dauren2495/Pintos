#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/init.h"
#include "threads/palloc.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);
struct semaphore fs_sema;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool valid_byte(void *p){
  struct thread *t = thread_current();
  if(p >= PHYS_BASE || p == NULL)
    return false;
  if(!pagedir_get_page(t->pagedir,p) && !page_lookup(p))
    return false;
  return true;
}

void exit(void){
  struct thread *t = thread_current();
  t->exit_status = -1;
  printf("%s: exit(%d)\n", t->name, t->exit_status);
  thread_exit();
}
  

void check_string(char *str){
  if(!valid_byte(str))
    exit();
  for(int i = 0; i < strlen(str) + 1; i++)
    if(!valid_byte(str + i)){
      exit();
    }
}

void check_buffer(void *buffer, size_t size){
  for(int i = 0; i < size; i++)
    if(!valid_byte(buffer + i))
      exit();
}

void check_ptr(void *ptr){
  check_buffer(ptr, 4);
}
    
  
void is_bad_args(int *p, int argc){
  bool bad = false;
  struct thread *t  = thread_current();
  if(argc == 0){
    if(p >= PHYS_BASE || p == NULL)
      bad = true;
    if(!bad)
      if(!pagedir_get_page(t->pagedir, p))
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
    t->exit_status = -1;
    printf("%s: exit(%d)\n", t->name, t->exit_status);
    thread_exit();
  }
    
}

struct fd_ *search_fd(struct thread *t, int fd){
    struct list_elem *e;
    struct fd_ *file_d = NULL;
    for( e = list_begin(&t->files); e != list_end(&t->files); e = list_next(e)){
      file_d = list_entry(e, struct fd_, elem);
      if(file_d->fd == fd)
	break;
    }
    if(file_d == NULL || file_d->fd != fd)
      return NULL;
    else
      return file_d;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    int *p = f->esp;
    struct thread *t = thread_current();
    check_ptr(f->esp);
    switch(*p){
    case SYS_HALT:
      {
	shutdown_power_off();
	break;
      }
    case SYS_EXIT:
      {
	check_ptr(p + 1);
	int *status = p + 1;
	f->eax = *status;
	t->exit_status = *status;
	printf("%s: exit(%d)\n", t->name, t->exit_status);
	thread_exit();
	break;
      }
    case SYS_WRITE:
      {
	check_ptr(p + 1);
	check_ptr(p + 2);
	check_ptr(p + 3);
	int fd = *(p + 1);
	const void **buffer = (p + 2);
	unsigned size = *(p + 3);
	check_buffer(*buffer, size);
	
	if(fd == 0)
	  ;//writing to stdin
	else if(fd == 1){    
	  putbuf(*buffer, size);
	}
	else {
	  struct fd_* file_d = search_fd(t, fd);
	  if(file_d != NULL)
	      f->eax = file_write(file_d->file, *buffer, size);
	  else
	    f->eax = -1;
	}
	break;
      }
    case SYS_CREATE:
      {
	check_ptr(p + 1);
	check_ptr(p + 2);
	const char** file = (const char**)(p+1);
	unsigned initial_size = *(unsigned*)(p+2);
	check_string(*file);
	f->eax = filesys_create(*file, initial_size);
	break;
      }
    case SYS_OPEN:
      {
	check_ptr(p + 1);
	const char **name = p + 1;
	check_string(*name);
	struct file *file = filesys_open(*name);
	if(!file)
	  f->eax = -1;
	else{
	  struct fd_* new_fd = calloc(1, sizeof(struct fd_));
	  new_fd->fd = t->next_fd;
	  new_fd->file = file;
	  list_push_back(&t->files, &new_fd->elem);
	  f->eax = t->next_fd++;
	}
	break;
      }
    case SYS_CLOSE:
      {
	check_ptr(p + 1);
	int fd = *(p + 1);
	struct fd_* file_d = search_fd(t, fd);
	if(file_d != NULL){
	  list_remove(&file_d->elem);
	  file_close(file_d->file);
	  free(file_d);
	}
	break;
      }

    case SYS_FILESIZE:
      {
	check_ptr(p + 1);
	int fd = *(p + 1);
	struct fd_* file_d = search_fd(t, fd);
	if(file_d != NULL)
	  f->eax = file_length(file_d->file);
	else
	  f->eax = 0;
	break;
      }
    case SYS_READ:
      {
	check_ptr(p + 1);
	check_ptr(p + 2);
	check_ptr(p + 3);
	int fd = *(p + 1);
	const void **buffer = p + 2;
	unsigned size = *(p + 3);
	check_buffer(*buffer, size);
	if(fd == 0){
	  f->eax = input_getc();
	}
	else{
	  struct fd_ *file_d = search_fd(t, fd);
	  if(file_d != NULL)
	    f->eax = file_read(file_d->file, *buffer, size);
	  else
	    f->eax = -1;
	  break;
	}
	break;
      }
    case SYS_REMOVE:
      {
	check_ptr(p + 1);
	const char **file = p + 1;
	check_string(*file);
	f->eax = filesys_remove(*file);
	break;
      }
    case SYS_SEEK:
      {
	check_ptr(p + 1);
	check_ptr(p + 2);
	int fd = *(p + 1);
	unsigned pos = *(p + 2);
	struct fd_* file_d = search_fd(t, fd);
	if(file_d != NULL)
	  file_d->file->pos = pos;
	break;
      }
    case SYS_TELL:
      {
	check_ptr(p + 1);
	int fd = *(p + 1);
	struct fd_* file_d = search_fd(t, fd);
	if(file_d != NULL)
	  f->eax = file_d->file->pos;
	break;
      }
    case SYS_EXEC:
      {
	// dummy implemetation, still didn't implement synchronization
	check_ptr(p + 1);
	const char **cmd_line = p + 1;
	check_string(*cmd_line);
	f->eax = process_execute(*cmd_line);
	sema_down(&t->wait);
	if(!t->load_child){
	  t->load_child = true;
	  f->eax = -1;
	}
	break;
      }
    case SYS_WAIT:
      {
	check_ptr(p + 1);
	int tid = *(p + 1);
	//printf("Process %s waits for tid %d\n",thread_current()->name, tid);
	f->eax = process_wait(tid);
	//printf("End of SYS_WAIT\n");
	break;
      }
    }
}
