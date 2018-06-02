#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"





/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt,
	    block_sector_t parent_sector)
{
  
  bool success =
    inode_create (sector, entry_cnt * sizeof (struct dir_entry), 1);

  if(success) {
    struct inode* dir_inodep = inode_open(sector);
    struct dir_entry self, parent;
    self.inode_sector = sector;
    parent.inode_sector = parent_sector;
    self.name[0]=parent.name[0]=parent.name[1]='.';
    self.name[1]=parent.name[2]='\0';
    self.in_use = parent.in_use = true;
    //self.is_dir = parent.is_dir = true;
    int entry_size = sizeof (struct dir_entry);
    //printf("(dir_create) self.inode_sector:%d, parent.inode_sector:%d\n\n",
    //	   self.inode_sector, parent.inode_sector);
    if(entry_size == inode_write_at(dir_inodep, &self, entry_size, 0) &&
       entry_size == inode_write_at(dir_inodep, &parent,
				    entry_size, entry_size))
    ;
    else success = false;

    inode_close(dir_inodep);
  }
  return success;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  //printf("\n(lookup) Search for entry with name:%s\n", name);
  //printf("in dir with inode at sector:%d\n",
  //	 dir->inode->sector);
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {
    //printf("entry.name:%s\n", e.name);
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  //  printf("(dir_add) dir->inode->sector:%d, name:%s, inode_sector:%d\n",
  //	 dir->inode->sector, name, inode_sector);
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;


  lock_acquire(&dir->inode->entries_lock);
  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL)) 
    goto done;
  
  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;

  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

  
 done:
  lock_release(&dir->inode->entries_lock);
  //lookup (dir, name, NULL, NULL);//for dbg, it prints out entries
  //until finding "name"
  return success;
}

/*new func. returns true if entry is for a directory, 
  false otherwise*/
bool
is_dir_by_inode(struct inode* pi){
  if(pi && pi->data.is_dir){
    return true;
  }
  return false;
}

/*new func. returns true if directory is empty,
  false otherwise*/
bool
is_dir_empty_by_inode(struct inode* pi){
  struct dir* pdir = dir_open(pi);
  pdir->pos=0;
  char name[NAME_MAX+1];
  if(!dir_readdir(pdir, name) || strcmp(name,".")){
    printf("(is_dir_empty) UNEXPECTED ERROR 1\n");
    dir_close(pdir);
    return false;
  }
  if(!dir_readdir(pdir, name) || strcmp(name,"..")){
    printf("(is_dir_empty) UNEXPECTED ERROR 2\n");
    dir_close(pdir);
    return false;
  }

  while(dir_readdir(pdir, name)) {
    struct dir_entry e;
    inode_read_at(pdir->inode, &e, sizeof e, pdir->pos - sizeof(e));
    if (e.in_use) {
      //printf("name of entry in use:%s\n", e.name);
      dir_close(pdir);
      return false;
    }
  }
  dir_close(pdir);
  return true;
  
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  lock_acquire(&dir->inode->entries_lock);
  
  if(*name == '/'){
    /*root*/
    goto done;
  }


  
  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs)){
    //printf("dir_remove: lookup returned false\n");
    goto done;
  }
  
  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL) {
    //printf("inode==NULL\n");
    goto done;
  }
  
  if(is_dir_by_inode(inode)) {
    // printf("\n%s IS DIRECTORY\ne.in_use:%d\n\n",
    //	   name, e.in_use);
    if(inode->open_cnt > 1) {
      //printf("inode->open_cnt >1\n");
      goto done;
    }

    if(!is_dir_empty_by_inode(inode)) {
      //printf("DIR is NOT EMPTY\n");
      goto done;
    }
    //printf("DIR is EMPTY\n");
    
  }

  

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  lock_release(&dir->inode->entries_lock);
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}
