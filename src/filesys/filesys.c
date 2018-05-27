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
  thread_current()->cwd  = dir_open_root();
  
  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (struct dir* dir, const char *name,
		off_t initial_size, bool is_dir) 
{
  ASSERT(dir);
  char* path = malloc(strlen(name)+1);
  strlcpy(path, name, strlen(name)+1);
  char* rest = path;
  char* token;
  //struct dir* parent_dir = dir;
  bool need_to_close_dir = false;
  if(*name == (char)'/') {
    need_to_close_dir = true;
    dir = dir_open_root();
  }
  for( token = strtok_r(path, "/", &rest); token!=NULL;
       token = strtok_r(NULL, "/", &rest) ) {
    //printf("\ntoken:%s\n", token);
    //printf("rest:%s\n", rest);
    if ((*rest) == '\0') {
      break;
    }
    //printf("\nnow token is: %s\n", token);
    struct inode* dir_inodep;
    //printf("dir->inode->sector:%d\n", dir->inode->sector);
    if(!dir_lookup(dir, token, &dir_inodep)){
      
      //printf("returning false bcs directory doesn't exist\n");
      //printf("directory:%s\n", token);
      free(path);
      return false;
    }
    if(need_to_close_dir){
      dir_close(dir);
    }
    dir = dir_open(dir_inodep);
    if (!dir) return false;
  }


  block_sector_t inode_sector = 0; bool success;
  if(is_dir) {
    char* new_dir_name = token;
    success = (dir!=NULL
	       && free_map_allocate(1, &inode_sector)
	       && dir_create(inode_sector, 16, dir->inode->sector)
	       && dir_add(dir, new_dir_name, inode_sector));
    
  }
  else {
    char* new_file_name = token;
    //printf("file_name: %s\n", file_name);
    success = (dir != NULL
	       && free_map_allocate (1, &inode_sector)
	       && inode_create (inode_sector, initial_size, is_dir)
	       && dir_add (dir, new_file_name, inode_sector));
  }
  //printf("(filesys_create) success:%d\n", success);
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);

  //dir_close (dir); //should we close it? if we do - t->cwd is lost
  //it was here by default
  //try:
  if(need_to_close_dir) {
    dir_close(dir);
  }

  
  free(path);
  //printf("\nreturning from filesys_create, success:%d\n\n", success);
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
  
  char* path = malloc(strlen(name)+1);
  strlcpy(path, name, strlen(name)+1);
  char* rest = path;
  char* token;
  //struct dir* parent_dir = dir;
  bool need_to_close_dir = false;
  struct dir* dir;
  if(*name == '/') {
    need_to_close_dir = true;
    dir = dir_open_root();
  }
  else {
    dir = thread_current()->cwd;
  }
  for( token = strtok_r(path, "/", &rest); token!=NULL;
       token = strtok_r(NULL, "/", &rest) ) {
    //printf("\ntoken:%s\n", token);
    //printf("rest:%s\n", rest);
    if ((*rest) == '\0') {
      break;
    }
    //printf("\nnow token is: %s\n", token);
    struct inode* dir_inodep;
    //printf("dir->inode->sector:%d\n", dir->inode->sector);
    if(!dir_lookup(dir, token, &dir_inodep)){
      
      //printf("returning false bcs directory doesn't exist\n");
      //printf("directory:%s\n", token);
      free(path);
      printf("FAIL because directory is not found\n");
      return NULL;
    }
    if(need_to_close_dir){
      dir_close(dir);
    }
    dir = dir_open(dir_inodep);
    if (!dir) {
      free(path);
      printf("FAIL because dir==NULL\n");
      return NULL;
    }
  }
  
  char * file_name = token;
  struct inode *inode = NULL;
  //printf("\n(filesys_open) dir->inode->sector:%d\n",
  //	 dir->inode->sector);
  
  if(*path=='/' && *file_name=='\0' && *(path+1)=='\0') {
    /*file_name is root*/
    if (dir->inode->sector == ROOT_DIR_SECTOR) {
      //printf("dir->inode->sector == ROOT_DIR_SECTOR\n");
      file_name = ".";
    }
    else {
      free(path);
      return NULL;
    }
  }
  //printf("file_name:%s\n", file_name);
  if (dir != NULL){
    dir_lookup (dir, file_name, &inode);
    //printf("(filesys_open) file:%s is found to have inode->sector:%d\n",
    //	   file_name, inode->sector);
  }
  else {
    free(path);
    return NULL;
  }
  if (need_to_close_dir) {
    dir_close (dir);
  }
  free(path);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{

  char* path = malloc(strlen(name)+1);
  strlcpy(path, name, strlen(name)+1);
  char* rest = path;
  char* token;
  //struct dir* parent_dir = dir;
  bool need_to_close_dir = false;
  struct dir* dir;
  if(*name == '/') {
    need_to_close_dir = true;
    dir = dir_open_root();
  }
  else {
    dir = thread_current()->cwd;
  }
  for( token = strtok_r(path, "/", &rest); token!=NULL;
       token = strtok_r(NULL, "/", &rest) ) {
    //printf("\ntoken:%s\n", token);
    //printf("rest:%s\n", rest);
    if ((*rest) == '\0') {
      break;
    }
    //printf("\nnow token is: %s\n", token);
    struct inode* dir_inodep;
    //printf("dir->inode->sector:%d\n", dir->inode->sector);
    if(!dir_lookup(dir, token, &dir_inodep)){
      
      //printf("returning false bcs directory doesn't exist\n");
      //printf("directory:%s\n", token);
      free(path);
      return false;
    }
    if(need_to_close_dir){
      dir_close(dir);
    }
    dir = dir_open(dir_inodep);
    if (!dir) {
      free(path);
      return false;
    }
  }
  
  char * file_name = token;

  //printf("REACHED HERE, file_name:%s, path:%s\n",
  //	 file_name, path);
  bool success=true;
  if(!file_name && (*path=='/')) {
    /*cannot remove root*/
    //printf("cannot remove root\n");
    success = false;
  }
  success = success && (dir != NULL) && dir_remove (dir, file_name);
  
  if(dir && need_to_close_dir) {
    dir_close (dir); 
  }
  

  free(path);
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
