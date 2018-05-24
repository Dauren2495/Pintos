#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length) {
    block_sector_t sector=-1;
    int direct_cnt_max = sizeof(inode->data.direct)/sizeof(block_sector_t);
    
    if(pos < BLOCK_SECTOR_SIZE * direct_cnt_max){
      sector = *(inode->data.direct + pos/BLOCK_SECTOR_SIZE) ;
      if (sector==NO_SECTOR)
	return -1;
      return sector;
    }
    

    else if(pos < BLOCK_SECTOR_SIZE * (direct_cnt_max+128)){
      static block_sector_t buf[128];
      if (inode->data.indirect == NO_SECTOR)
	return -1;
      block_read(fs_device, inode->data.indirect, buf);
      sector = *(buf+pos/BLOCK_SECTOR_SIZE-direct_cnt_max);
      if (sector==NO_SECTOR)
	return -1;
      return sector;
    }

    else if(pos < BLOCK_SECTOR_SIZE * (direct_cnt_max+128+128*128)){
      if (inode->data.d_indirect == NO_SECTOR)
	return -1;
      static block_sector_t d_indirect_buf[128];
      block_read(fs_device, inode->data.d_indirect, d_indirect_buf);
      size_t indirect_index = (pos/BLOCK_SECTOR_SIZE - direct_cnt_max-128)/128;
      ASSERT(indirect_index<128);
      static block_sector_t indirect_buf[128];
      if (d_indirect_buf[indirect_index] == NO_SECTOR)
	return -1;
      block_read(fs_device, d_indirect_buf[indirect_index], indirect_buf);
      sector = indirect_buf[(pos/BLOCK_SECTOR_SIZE - direct_cnt_max-128)%128];
      if (sector == NO_SECTOR)
	return -1;
      return sector;
    }
    else
      NOT_REACHED();
      
  }
 
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}



/*new. Allocates CNT contiguous sectors
starting from PS.
Returns true on success, false otherwise*/
static bool
allocate_sectors_cntg(int cnt, block_sector_t* ps) {
  if (free_map_allocate (cnt, ps)) { 
    /*put zero into blocks*/
    if(cnt>0) {
      static char zeros[BLOCK_SECTOR_SIZE];
      
      int i;
      
      for (i = 0; i < cnt; i++) 
	block_write (fs_device, *ps+i, zeros);
    }

    return true;
  }
  else return false;
}

/*new. Direct allocation. Returns true on success, false otherwise*/
static bool
allocate_sectors_directly(size_t cnt_direct, block_sector_t* ps) {
  bool success = true;
  while(cnt_direct>0) {
    if(allocate_sectors_cntg(cnt_direct, ps)){
      /*sectors were allocated contiguously*/
      // (*ps) holds first sector of CNT contiguous blocks 
      // but *(ps+1) ... *(ps+cnt-1) do not. Fix it:
      block_sector_t s = (*ps) + 1;
      for(size_t i = 1; i < cnt_direct; i++){
	*(ps+i) = s;
	s++;
      }
      break;
    }
    else {
      /*failed to allocate cnt_direct contiguous blocks
	so allocate just 1*/
      if(!allocate_sectors_cntg(1, ps)){
	success = false;
	break;
      }
      ps++;
      cnt_direct--;
      continue;
    }
  }
  return success;

}


/*returns true on success, false otherwise*/
static bool
allocate_sectors_indirectly(int cnt_indirect, block_sector_t* pindirect) { 
  if(!allocate_sectors_cntg(1, pindirect))
    return false;
  static block_sector_t buf[128] = {0};
  if(!allocate_sectors_directly(cnt_indirect, buf)){
    return false;
  }
  block_write(fs_device, *pindirect, buf);
  return true;
}




/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  //printf("\n\nIn INODE_CREATE\n\n");
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)  {
    size_t sectors = bytes_to_sectors (length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    /*
      if (free_map_allocate (sectors, &disk_inode->start)) 
      {
      block_write (fs_device, sector, disk_inode);
      if (sectors > 0) 
      {
      static char zeros[BLOCK_SECTOR_SIZE];
      size_t i;
              
      for (i = 0; i < sectors; i++) 
      block_write (fs_device, disk_inode->start + i, zeros);
      }
      success = true; 
      }
    */
    
    size_t max_direct_cnt = sizeof(disk_inode->direct)/sizeof(block_sector_t);
    /*number of sectors pointed directly, indirectly, 
      and doubly indirectly*/
    size_t cnt_direct, cnt_indirect, cnt_d_indirect;
    if (sectors <= max_direct_cnt) {
      cnt_direct = sectors;
      cnt_indirect = cnt_d_indirect = 0;
    }
    else if (sectors <= max_direct_cnt + 128){
      cnt_direct = max_direct_cnt;
      cnt_indirect = sectors - max_direct_cnt;
      cnt_d_indirect = 0;
    }
    else if (sectors <= max_direct_cnt +128*128) {
      cnt_direct = max_direct_cnt;
      cnt_indirect = 128;
      cnt_d_indirect = sectors - 128 - max_direct_cnt;
    }
    else {
      //printf("INODE_CREATE FAILURE: file is too long\n");
      return false;
    }

    
    
    /*allocation of sectors:*/
    block_sector_t* ps = disk_inode->direct;

    if (cnt_direct>0) {
      if(!allocate_sectors_directly(cnt_direct, ps)){
	return false;
      }
    }
    
    if(cnt_indirect>0) {
      if (!allocate_sectors_indirectly(cnt_indirect, &disk_inode->indirect))
	return false;
      
    }

    
    if(cnt_d_indirect>0) {
      if(!allocate_sectors_cntg(1, &disk_inode->d_indirect))
	return false;
      static block_sector_t buf[128]={0};
      int i = 0;
      while(cnt_d_indirect>128) {
	ASSERT(i<128);
	if (!allocate_sectors_indirectly(128, buf+i))
	  return false;
	i++;
	cnt_d_indirect-=128;
      }
      if(cnt_d_indirect>0) {
	if (!allocate_sectors_indirectly(cnt_d_indirect, buf+i))
	  return false;
	cnt_d_indirect = 0;
      }

      block_write(fs_device, disk_inode->d_indirect, buf);
    }
    
    block_write (fs_device, sector, disk_inode);
    success = true;
    //printf("INODE_CREATE SUCCESS\n");
  }

      
  /*do not forget to zero out allocated sectors*/
  /*and manage failures properly*/
  
  free (disk_inode);
  //printf("INODE_CREATE just before returning\n");
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{

  //printf("\n\nREACHED INODE_CLOSE\n\n");
  
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          size_t max_direct_cnt = sizeof(inode->data.direct)/sizeof(block_sector_t);
	  for(size_t i = 0;i < max_direct_cnt;i++) {
	    if(inode->data.direct[i] == NO_SECTOR)
	      goto ending;
	    free_map_release (inode->data.direct[i], 1);
	  }

	  if(inode->data.indirect == NO_SECTOR)
	    goto ending;
	  static block_sector_t buf[128];
	  block_read(fs_device, inode->data.indirect, buf);
	  free_map_release(inode->data.indirect, 1);
	  for(size_t i=0; i<128; i++) {
	    if(buf[i] == NO_SECTOR)
	      goto ending;
	    free_map_release(buf[i], 1);
	  }
	  
	  if(inode->data.d_indirect == NO_SECTOR)
	    goto ending;
	  static block_sector_t d_indirect_buf[128];
	  block_read(fs_device, inode->data.d_indirect, d_indirect_buf);
	  free_map_release(inode->data.d_indirect, 1);
	  for(int i = 0; i<128; i++) {
	    if(d_indirect_buf[i] == NO_SECTOR)
	      goto ending;
	    block_read(fs_device, d_indirect_buf[i], buf);
	    free_map_release(d_indirect_buf[i], 1);
	    for(int j=0; j<128; j++) {
	      if(buf[j] == NO_SECTOR)
		goto ending;
	      free_map_release(buf[j], 1);
	    }

	  }

	  /*
          free_map_release (inode->data.start,
	                    bytes_to_sectors (inode->data.length));
	  */ 
        }
    ending:
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
OLD_inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}


off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}










/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  //printf("deny inode\n");
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
