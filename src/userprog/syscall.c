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
#include "vm/swap.h"


static void syscall_handler (struct intr_frame *);
extern uint8_t* stack_end;
extern struct swap swap;


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool valid_byte(void *p){
  struct thread *t = thread_current();
  if(PHYS_BASE > p && p > stack_end)
    return true;
  else if(p >= PHYS_BASE || p == NULL)
    return false;
  else if(!pagedir_get_page(t->pagedir,p) && !page_lookup(&t->pages, p))
    return false;
  else
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

void pin_page(uint32_t start, unsigned size)
{
  uint32_t end = start + size;
  int count = (end / PGSIZE) - (start / PGSIZE) + 1;
  struct page *p;
  struct thread *t = thread_current();
  for(int i = 0; i < count; i++)
    {
      p = page_lookup(&t->pages, start + i * PGSIZE);
      if(!p)
	{
	  p = calloc(sizeof(struct page), 1);
	  p->upage = (start + i * PGSIZE) & !PGMASK;
	  p->zero_bytes = PGSIZE;
	  p->writable = true;
	  hash_insert(&t->pages, &p->hash_elem);
	  if(p->upage < stack_end)
	    stack_end = p->upage;     
	}
      p->lock = true;

      if(!pagedir_get_page(t->pagedir, p->upage))
	{
	  void *kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	  
	  p->kpage = (uint8_t*) kpage;
	  if(p->swap)
	    swap_read(&swap, p);
	  else if(p->file != NULL)
	    {
	      file_seek(p->file, p->ofs);
	      if (file_read (p->file, kpage, p->read_bytes) != (int) p->read_bytes)
		{
		  palloc_free_page (kpage);
		}
	      memset (kpage + p->read_bytes, 0, p->zero_bytes);
	    }
	  else
	    memset (kpage + p->read_bytes, 0, p->zero_bytes);
	  pagedir_set_page(t->pagedir, (void*)p->upage, kpage,p->writable);
	}
    }
}

void unpin_page(uint32_t start, unsigned size)
{
  uint32_t end = start + size;
  int count = (end / PGSIZE) - (start / PGSIZE) + 1;
  struct page *p;
  for(int i = 0; i < count; i++)
    {
      p = page_lookup(&thread_current()->pages, start + i * PGSIZE);
      p->lock = false;
    }
}



static bool
isdir(struct thread* t, int fd) {
  struct fd_* file_d = search_fd(t, fd);
  if(file_d) {
    return file_d->file->inode->data.is_dir;
  }
  else{
    return false;
  }
  
}

static int
inumber(struct thread* t, int fd){
  struct fd_* file_d = search_fd(t, fd);
  if(file_d) {
    return file_d->file->inode->sector;
  }
  else{
    return 0;
  }

}

static bool
readdir(struct thread *t, int fd, char *name) {
  struct fd_* file_d = search_fd(t, fd);
  if (file_d && isdir(t, fd)) {
    struct dir_entry e;
    int bytes_read;
    while(bytes_read = file_read(file_d->file, &e, sizeof e)) {
      if (bytes_read!=sizeof e) {
	return false;
      }
      if(!e.in_use
	 || !strcmp(e.name,".")
	 || !strcmp(e.name,"..")) {
	continue;
      }
      strlcpy(name, e.name, sizeof(e.name));
      return true;
    }
  }
  return false;
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
	if(isdir(t, fd)) {
	  f->eax=-1;
	  break;
	}
	const void **buffer = (p + 2);
	unsigned size = *(p + 3);
	check_buffer(*buffer, size);
	pin_page(*buffer, size);
	//pin_page(f->esp, 0);
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
	unpin_page(*buffer, size);
	//unpin_page(f->esp, 0);
	break;
      }
    case SYS_CREATE:
      {
	check_ptr(p + 1);
	check_ptr(p + 2);
	const char** file = (const char**)(p+1);
	unsigned initial_size = *(unsigned*)(p+2);
	check_string(*file);
	if(**file == '\0'){
	  f->eax=0;
	  break;
	}
	//printf("\n(SYS_CREATE) t->cwd->inode->sector:%d\n",
	//       t->cwd->inode->sector);
	f->eax = filesys_create(*file, initial_size, 0);
	break;
      }
    case SYS_OPEN:
      {
	check_ptr(p + 1);
	const char **name = p + 1;
	check_string(*name);
	if(**name=='\0'){
	  f->eax=-1;
	  break;
	}
	struct file *file = filesys_open(*name);
	//printf("(SYS_OPEN) name:%s\n", *name);
	if(!file){
	  //printf("(SYS_OPEN) file==NULL\n");
	  f->eax = -1;
	}
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
	pin_page(*buffer, size);
	//pin_page(f->esp, 0);
	if(fd == 0){
	  f->eax = input_getc();
	}
	else{
	  struct fd_ *file_d = search_fd(t, fd);
	  if(file_d != NULL)
	    f->eax = file_read(file_d->file, *buffer, size);
	  else
	    f->eax = -1;
	}
	unpin_page(*buffer, size);
	//unpin_page(f->esp, 0);
	break;
      }
    case SYS_REMOVE:
      {
	check_ptr(p + 1);
	const char **file = p + 1;
	check_string(*file);
	f->eax = filesys_remove(*file);
	//printf("EXITED filesys_remove, f->eax:%d\n", f->eax);
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
    case SYS_MMAP:
      {
	check_ptr(p + 1);
	check_ptr(p + 2);
	int fd = *(p + 1);
	uint8_t *addr = *(p + 2);
	struct fd_ *file_d = search_fd(t, fd);
	if(!fd){
	  f->eax = -1;
	  break;
	}
	size_t file_size = file_length(file_d->file);
	size_t pages = (file_size / PGSIZE) + 1;
	size_t last_read = file_size % PGSIZE;
	uint8_t *last_addr = addr + pages * PGSIZE;
	// account for stack overlap
	if( addr == NULL || file_size == 0 || (unsigned) addr % PGSIZE != 0 ||  \
	    pagedir_get_page(t->pagedir, addr) || page_lookup(&t->pages, addr) || \
	    pagedir_get_page(t->pagedir, last_addr) || page_lookup(&t->pages, last_addr)){
	  f->eax = -1;
	  break;
	}
	struct map *m = calloc(sizeof(struct map), 1);
	m->addr = addr;
	m->cnt = pages;
	m->mapid = t->next_fd++;
	list_push_back(&t->map, &m->list_elem);

	for(int i = 0; i < pages; i++)
	  {
	    struct page *p = calloc(sizeof(struct page), 1);
	    p->upage = addr;
	    p->file = file_reopen(file_d->file);
	    p->writable = file_d->file->inode->deny_write_cnt > 0 ? false : true;
	    p->ofs = i * PGSIZE;
	    p->kpage = NULL;
	    if(i != (pages - 1)){
	      p->read_bytes  = PGSIZE;
	      p->zero_bytes = 0;
	    }
	    else{
	      p->read_bytes = last_read;
	      p->zero_bytes = PGSIZE - last_read;
	    }
	    hash_insert(&t->pages, &p->hash_elem);
	    addr += PGSIZE;
	    //printf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
	  }
	f->eax = m->mapid;
	break;
      }
    case SYS_MUNMAP:
      {
	//printf("-----------------------------remove\n");
	check_ptr(p + 1);
	unsigned mapid = *(p + 1);
	uint8_t *addr;
	struct list_elem *e;
	struct map *m;
	for( e = list_begin(&t->map); e != list_end(&t->map); e = list_next(e))
	  {
	    m = list_entry(e, struct map, list_elem);
	    if(m->mapid == mapid)
	      break;
	  }
	for(int i = 0; i < m->cnt; i++)
	  {
	    struct page *p = page_lookup(&t->pages, m->addr + i * PGSIZE);
	    if(pagedir_is_dirty(t->pagedir, p->upage))
	      file_write_at(p->file, p->upage, PGSIZE, p->ofs);
	    pagedir_clear_page(t->pagedir, p->upage);
	    palloc_free_page(p->kpage);
	    hash_delete(&t->pages, &p->hash_elem);
	    free(p->file);
	    free(p);
	  }
	list_remove(&m->list_elem);
	free(m);
	break;
      }




    case SYS_MKDIR:
      {
	check_ptr(p+1);
	check_string(*(p+1));
	const char* dir_name = *(p+1);
	if (!(*dir_name)) {
	  f->eax=0;
	  break;
	}
	
	/*assume dir is relative*/
	//printf("\n(SYS_MKDIR) t->cwd->inode->sector:%d\n",
	//     t->cwd->inode->sector);
	f->eax = filesys_create(dir_name, 0, 1);
	break;
      }

    case SYS_CHDIR:
      {
	check_ptr(p+1);
	check_string(*(p+1));
	const char* full_name = *(p+1);
	char* short_name;
	
	if (!(*full_name)) {
	  f->eax=0;
	  break;
	}

	struct dir* pdir;
	/*parsing dir_name*/
	char* path = malloc(strlen(full_name)+1);
	strlcpy(path, full_name, strlen(full_name)+1);
	char* rest = path;
	char* token;
	if(*full_name == (char)'/') {
	  dir_close(thread_current()->cwd);
	  pdir = dir_open_root();
	}
	else {
	  pdir = thread_current()->cwd;
	}
	for( token = strtok_r(path, "/", &rest); token!=NULL;
	     token = strtok_r(NULL, "/", &rest) ) {
	  if ((*rest) == '\0') {
	    break;
	  }

	  struct inode* dir_inodep;
	  if(!dir_lookup(pdir, token, &dir_inodep)){
	    printf("FAIL A\n");
	    free(path);
	    f->eax=0;
	    break;
	  }
	  
	  if(!strcmp(token,"..")){
	    dir_close(pdir);
	  }
	  
	  pdir = dir_open(dir_inodep);
	  if (!pdir) {
	    free(path);
	    goto fail;
	  }
	}
	if(!token) {
	  short_name=NULL;
	}
	else{
	  short_name = malloc(strlen(token)+1);
	  strlcpy(short_name, token, strlen(token)+1);
	}
	free(path);

	if(!short_name && *full_name=='/') {
	  /*full_name is root*/
	  if (pdir->inode->sector == ROOT_DIR_SECTOR) {
	    //printf("dir->inode->sector == ROOT_DIR_SECTOR\n");
	    short_name = malloc(strlen(".")+1);
	    strlcpy(short_name, ".", strlen(".")+1);
	  }
	  else {
	    f->eax=0;
	    break;
	  }
	}

	if(!pdir) {
	  free(short_name);
	  f->eax=0;
	  break;
	}
	/*assume name is relative*/
	struct inode* pdir_inode;
	if(!dir_lookup(pdir, short_name, &pdir_inode)){
	  f->eax=0;
	  break;
	}

	//if(!strcmp(short_name, "..")){
	  //printf("SHORT_NAME is ..\n");
	  dir_close(t->cwd);
	  //printf("CLOSED t->cwd\n");
	  //}
	t->cwd=dir_open(pdir_inode);
	//printf("OPENED pdir_inode\n");
	if(!t->cwd) {
	fail:
	  //printf("FAIL\n");
	  f->eax=0;
	  break;
	}
	else {
	  f->eax=1;
	  break;
	}
	
      }


    case SYS_ISDIR:
      {
	check_ptr(p + 1);
	int fd = *(p + 1);
	f->eax = isdir(t, fd);
	break;

      }
      
      
    case SYS_INUMBER:
      {
	check_ptr(p + 1);
	int fd = *(p + 1);
	f->eax = inumber(t, fd);
	break;
      }

    case SYS_READDIR:
      {
	check_ptr(p+1);
	check_ptr(p+2);
	check_string(*(p+2));
	int fd = *(p+1);
	char *name = *(p+2);
	f->eax = readdir(t, fd, name);
	break;
      }
      
    }
}
