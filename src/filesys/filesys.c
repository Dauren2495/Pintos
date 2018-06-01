#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  
  if (format) 
    do_format ();
  
  free_map_open ();

  thread_current()->cwd  = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}



/*new func. extracts file/directory name and its parent
returns true on success, false on failure*/
static bool
get_dir_and_name(const char *fullname, struct dir** ppdir,
		 char** pname, bool* pneed_to_close_dir){
  char* path = malloc(strlen(fullname)+1);
  strlcpy(path, fullname, strlen(fullname)+1);
  char* rest = path;
  char* token;
  //struct dir* parent_dir = dir;
  if(*fullname == (char)'/') {
    *pneed_to_close_dir = true;
    *ppdir = dir_open_root();
  }
  else {
    *pneed_to_close_dir = false;//so as to not lose cwd
    *ppdir = thread_current()->cwd;
  }
  for( token = strtok_r(path, "/", &rest); token!=NULL;
       token = strtok_r(NULL, "/", &rest) ) {
    if ((*rest) == '\0') {
      break;
    }

    struct inode* dir_inodep;
    if(!dir_lookup(*ppdir, token, &dir_inodep)){
      free(path);
      return false;
    }
    if(*pneed_to_close_dir){
      dir_close(*ppdir);
    }
    *ppdir = dir_open(dir_inodep);
    if (!*ppdir) {
      free(path);
      return false;
    }
  }
  if(!token) {
    *pname=NULL;
  }
  else{
    *pname = calloc(1, strlen(token)+1);
    strlcpy(*pname, token, strlen(token)+1);
  }
  free(path); 
  return true;

}


/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name,
		off_t initial_size, bool is_dir) 
{
  
  bool need_to_close_dir; struct dir* pdir;
  block_sector_t inode_sector = 0; bool success;
  if(is_dir) {
    char* new_dir_name;
    if(!get_dir_and_name(name, &pdir, &new_dir_name,
			 &need_to_close_dir)) {
      //no need to free new_dir_name
      return false;
    }
    success = (pdir!=NULL
	       && free_map_allocate(1, &inode_sector)
	       && dir_create(inode_sector, 16, pdir->inode->sector)
	       && dir_add(pdir, new_dir_name, inode_sector));
    free(new_dir_name);
    
  }
  else {
    char* new_file_name;
    if(!get_dir_and_name(name, &pdir, &new_file_name,
			 &need_to_close_dir)) {
      //no need to free new_file_name
      return false;
    }
    success = (pdir != NULL
	       && free_map_allocate (1, &inode_sector)
	       && inode_create (inode_sector, initial_size, is_dir)
	       && dir_add (pdir, new_file_name, inode_sector));
    free(new_file_name);
  }
  
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  
  if(need_to_close_dir) {
    /*need to close, because it was temporarily opened
     just for creating a file/directory in it*/
    dir_close(pdir);
  }

    
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  
  bool need_to_close_dir;
  struct dir* pdir; char* file_name;
  if(!get_dir_and_name(name, &pdir, &file_name, &need_to_close_dir)){
    return NULL;
  }
  
  struct inode *inode = NULL;
  
  if(!file_name && *name=='/') {
    /*file_name is root*/
    if (pdir->inode->sector == ROOT_DIR_SECTOR) {
      //printf("dir->inode->sector == ROOT_DIR_SECTOR\n");
      file_name = malloc(strlen(".")+1);
      strlcpy(file_name, ".", strlen(".")+1);
    }
    else {
      return NULL;
    }
  }
  if (pdir != NULL){
    dir_lookup (pdir, file_name, &inode);
  }
  else {
    free(file_name);
    return NULL;
  }
  if (need_to_close_dir) {
    dir_close (pdir);
  }
  free(file_name);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //printf("REACHED filesys_remove %s\n", name);
  bool need_to_close_dir;
  struct dir* pdir; char* file_name;
  if(!get_dir_and_name(name, &pdir, &file_name, &need_to_close_dir)){
    //printf("(filesys_remove) FAILED to parse\n");
    return false;
  }

  bool success=true;
  if(!file_name && (*name=='/')) {
    /*cannot remove root*/
    //printf("cannot remove root\n");
    success = false;
  }
  //printf("success1:%d\n", success);
  success = success && (pdir != NULL) && dir_remove (pdir, file_name);
  //printf("pdir!=NULL:%d, file_name:%s, success2:%d\n",
  //	 pdir!=NULL, file_name, success);
  if(pdir && need_to_close_dir) {
    dir_close (pdir); 
  }
  
  free(file_name);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16, ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
