#include <debug.h>
#include <stdio.h>
#include <string.h>

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init(bool format)
{
    fs_device = block_get_role(BLOCK_FILESYS);
    if (fs_device == NULL) {
        PANIC("No file system device found, can't initialize file system.");
    }

    inode_init();
    free_map_init();

    if (format) {
        do_format();
    }

    free_map_open();
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done(void)
{
    free_map_close();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create(const char *name, off_t initial_size, bool dirBool)
{
    block_sector_t inode_sector = 0;
    block_sector_t prev_dir = thread_current()->curr_dir;
    char *parsed_name = parse_file_name(name);
    if(parsed_name == NULL || strlen(parsed_name) == 0) return NULL;
    struct file *dir = dir_open_current();
    //Called with the fill path name (don't put parsing in your syscall) When you read end directory, call inode create
    //Call dir_open until you get to the end, at the end, create an inode (create a separate parse function)

    bool success = false;

    if(dirBool) {
        success = strlen(parsed_name) == 0? false : (dir != NULL 
                                                && free_map_allocate(1, &inode_sector)
                                                && dir_create(inode_sector, initial_size)
                                                && dir_add(dir, parsed_name, inode_sector));
    }
    else {
        success = (dir != NULL
                    && free_map_allocate(1, &inode_sector)
                    && inode_create(inode_sector, initial_size, dirBool) //Modified to accept dir param 
                    && dir_add(dir, parsed_name, inode_sector)); //Modify this to work with directories
    }

    if (!success && inode_sector != 0) {
        free_map_release(inode_sector, 1);
    }
    dir_close(dir);
    thread_current()->curr_dir = prev_dir;

    return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open(const char *name)
{
    // special case to open root node
    if(strcmp(name, "/") == 0) {
        return dir_open_root();
    }
    block_sector_t prev_dir = thread_current()->curr_dir;
    char *parsed_name = parse_file_name(name);
    if(parsed_name == NULL || strlen(parsed_name) == 0) return NULL;
    struct file *dir = dir_open_current();
    struct inode *inode = NULL;

    if (dir != NULL) {
        dir_lookup(dir, parsed_name, &inode);
    }
    dir_close(dir);
    thread_current()->curr_dir = prev_dir;

    return file_open(inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove(const char *name)
{
    if(strcmp(name, "/") == 0) return false; // auto-fail removal if removing root
    block_sector_t prev_dir = thread_current()->curr_dir;
    char *parsed_name = parse_file_name(name);
    if(parsed_name == NULL || strlen(parsed_name) == 0) return NULL;
    struct file *dir = dir_open_current();
    bool success = dir != NULL && dir_remove(dir, name);

    dir_close(dir);
    thread_current()->curr_dir = prev_dir;

    return success;
}

/* Formats the file system. */
static void
do_format(void)
{
    printf("Formatting file system...");
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16)) {
        PANIC("root directory creation failed");
    }
    free_map_close();
    printf("done.\n");
}

//Originally: inode is just a sector on a disk we know stores data in a specific way (inode is max 512 bytes)
//Directories needs to contain files (needs to contain files and directories). Needs to have info (4 byte sector number) on where to find the inodes within directory