#include <list.h>
#include <stdio.h>
#include <string.h>

#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "threads/thread.h"

/* Creates a directory with space for ENTRY_CNT entries in the
 * given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create(block_sector_t sector, size_t entry_cnt)
{
    bool success = inode_create(sector, entry_cnt * sizeof(struct dir_entry), true);
    if(success) {
        // open newly created directory and add '.' and '..' entries for the current and parent directories.
        struct file *dir = dir_sector_open(sector);
        block_sector_t parent_sector = (sector == ROOT_DIR_SECTOR)? ROOT_DIR_SECTOR : thread_current()->curr_dir; // parent is ROOT if current is ROOT
        success = dir != NULL && dir_add(dir, ".", sector) && dir_add(dir, "..", parent_sector);
        dir_close(dir);
        if(sector == ROOT_DIR_SECTOR) {
            thread_current()->curr_dir = ROOT_DIR_SECTOR;
        }
    }
    
    return success; //Modified to pass in "true" for last param to create directory
}

/* Opens and returns the directory for the given INODE, of which
 * it takes ownership.  Returns a null pointer on failure. */
struct file *
dir_open(struct inode *inode)
{
    return file_open(inode);
}

/* Opens the root directory and returns a directory for it.
 * Return true if successful, false on failure. */
struct file *
dir_open_root(void)
{
    return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
 * Returns a null pointer on failure. */
struct file *
dir_reopen(struct file *dir)
{
    return dir_open(inode_reopen(dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close(struct file *dir)
{
    if (dir != NULL) {
        // thread_current()->curr_dir = dir->parent;
        inode_close(dir->inode);
        free(dir);
    }
}

/* Opens and returns a directory directly from the block sector */
struct file *dir_sector_open(block_sector_t sector) {
    struct inode *dir_inode = inode_open(sector);
    struct file *dir = dir_open(dir_inode);
    return dir;
}

/* Opens and returns the current directory */
struct file *dir_open_current() {
    return dir_sector_open(thread_current()->curr_dir);
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode(struct file *dir)
{
    return dir->inode;
}

/* Searches DIR for a file with the given NAME.
 * If successful, returns true, sets *EP to the directory entry
 * if EP is non-null, and sets *OFSP to the byte offset of the
 * directory entry if OFSP is non-null.
 * otherwise, returns false and ignores EP and OFSP. */
//Might need to be changed for looking for entries not in root?
static bool
lookup(const struct file *dir, const char *name,
       struct dir_entry *ep, off_t *ofsp)
{
    struct dir_entry e;
    size_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
         ofs += sizeof e) {
        if (e.in_use && !strcmp(name, e.name)) {
            if (ep != NULL) {
                *ep = e;
            }
            if (ofsp != NULL) {
                *ofsp = ofs;
            }
            return true;
        }
    }
    return false;
}

/* Searches DIR for a file with the given NAME
 * and returns true if one exists, false otherwise.
 * On success, sets *INODE to an inode for the file, otherwise to
 * a null pointer.  The caller must close *INODE. */
//Might need to be changed for looking for entries not in root?
bool
dir_lookup(const struct file *dir, const char *name,
           struct inode **inode)
{
    struct dir_entry e;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    if (lookup(dir, name, &e, NULL)) {
        *inode = inode_open(e.inode_sector);
    } else {
        *inode = NULL;
    }

    return *inode != NULL;
}

//Given a relative or absolute directory, parses it and returns a directory for it
struct file* parse_dir(const char* dir){
  
    //String to parse (makes sure strtok doesn't mess it up)
    // char* directory = palloc_get_page(0);
    // strlcpy(directory, dir, strlen(dir));
    char* directory;
    directory = palloc_get_page(0);
    strlcpy(directory, dir, strlen(dir) + 1);

    //Store current directory
    struct file* currentDirectory;
    if(dir[0] == '/'){
      //Absolute directory
      if(strlen(dir) == 1) return NULL;
      currentDirectory = dir_open_root();
      memmove(directory, directory + 1, strlen(directory));
    //   directory = strtok_r(directory, "/", &directory); //Parse the backline
    } else {
      //Current directory (have to store this info)
      currentDirectory = dir_open_current();
    }

    //Parse until end
    while(strlen(directory) > 0){
      char* nextElmName = strtok_r(directory, "/", &directory); //Parse the next element in directory
      if(strlen(nextElmName) == 0) {
        return NULL;
      }
      struct inode* nextDirEntry = NULL;

      //TODO: All dir functions from userprog only work with the root directory, either modify them or create new ones
      if(!dir_lookup(currentDirectory, nextElmName, &nextDirEntry)){return NULL;} //Lookup next element. If lookup failed, return NULL
      dir_close(currentDirectory);
      currentDirectory = dir_open(nextDirEntry); //Optained inode for next element, open dir from it
      thread_current()->curr_dir = nextDirEntry->sector;
    }

    return currentDirectory;
}

void parse_file_path(char *file_path) {
    char* slashPosition = strchr(file_path, '/');

    if (slashPosition != NULL) {
        ++slashPosition;
        memmove(file_path, slashPosition, strlen(slashPosition) + 1);
    }
}

//Given a relative or absolute directory, goes into the last file and returns name of the file 
struct file* parse_file_name(const char* file) {
  
    //String to parse (makes sure strtok doesn't mess it up)
    // char* filepath = palloc_get_page(0);
    char* filepath;
    filepath = palloc_get_page(0);
    strlcpy(filepath, file, strlen(file) + 1);

    //Store current directory
    struct file* currentDirectory;

    if(filepath[0] == '/'){
      //Absolute directory
      currentDirectory = dir_open_root();
      thread_current()->curr_dir = ROOT_DIR_SECTOR;
      memmove(filepath, filepath + 1, strlen(filepath));
    } else {
      //Current directory (have to store this info)
      currentDirectory = dir_open_current();
    }

    //Parse until end
    while(strlen(filepath) > 0){
      char* nextElmName = strtok_r(filepath, "/", &filepath); //Parse the next element in directory
      if(strlen(nextElmName) == 0) {
        return NULL;
      }
      else if (strlen(filepath) == 0) {
        return nextElmName;
      }
      else {
        struct inode* nextDirEntry = NULL;
        //TODO: All dir functions from userprog only work with the root directory, either modify them or create new ones
        if(!dir_lookup(currentDirectory, nextElmName, &nextDirEntry)){return NULL;} //Lookup next element. If lookup failed, return NULL
        dir_close(currentDirectory);
        currentDirectory = dir_open(nextDirEntry); //Optained inode for next element, open dir from it
        thread_current()->curr_dir = nextDirEntry->sector;
      }
      
    }

    return NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
 * file by that name.  The file's inode is in sector
 * INODE_SECTOR.
 * Returns true if successful, false on failure.
 * Fails if NAME is invalid (i.e. too long) or a disk or memory
 * error occurs. */
bool
dir_add(struct file *dir, const char *name, block_sector_t inode_sector)
{
    struct dir_entry e;
    off_t ofs;
    bool success = false;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Check NAME for validity. */
    if (*name == '\0' || strlen(name) > NAME_MAX) {
        return false;
    }

    /* Check that NAME is not in use. */
    if (lookup(dir, name, NULL, NULL)) {
        goto done;
    }

    /* Set OFS to offset of free slot.
     * If there are no free slots, then it will be set to the
     * current end-of-file.
     *
     * inode_read_at() will only return a short read at end of file.
     * Otherwise, we'd need to verify that we didn't get a short
     * read due to something intermittent such as low memory. */
    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
         ofs += sizeof e) {
        if (!e.in_use) {
            break;
        }
    }

    /* Write slot. */
    e.in_use = true;
    strlcpy(e.name, name, sizeof e.name);
    e.inode_sector = inode_sector;
    success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
    return success;
}

/* Removes any entry for NAME in DIR.
 * Returns true if successful, false on failure,
 * which occurs only if there is no file with the given NAME. */
bool
dir_remove(struct file *dir, const char *name)
{
    struct dir_entry e;
    struct inode *inode = NULL;
    bool success = false;
    off_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Find directory entry. */
    if (!lookup(dir, name, &e, &ofs)) {
        goto done;
    }

    /* Open inode. */
    inode = inode_open(e.inode_sector);
    if (inode == NULL) {
        goto done;
    }
    
    /* if inode correponds to a directory and it is not empty, do not delete it, otherwise continue as normal */
    if(inode->data.directory) {
        char name[NAME_MAX + 1];
        struct file *dir = dir_open(inode);
        if(!dir_readdir(dir, name)) {
            dir_close(dir);
            goto done; 
        }
    }

    /* Erase directory entry. */
    e.in_use = false;
    if (inode_write_at(dir->inode, &e, sizeof e, ofs) != sizeof e) {
        goto done;
    }

    /* Remove inode. */
    inode_remove(inode);
    success = true;

done:
    inode_close(inode);
    return success;
}

/* Reads the next directory entry in DIR and stores the name in
 * NAME.  Returns true if successful, false if the directory
 * contains no more entries. */
bool
dir_readdir(struct file *dir, char name[NAME_MAX + 1])
{
    struct dir_entry e;

    while (inode_read_at(dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
        dir->pos += sizeof e;
        if (e.in_use) {
            strlcpy(name, e.name, NAME_MAX + 1);
            return true;
        }
    }
    return false;
}
