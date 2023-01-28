#include "helpers/storage.h"
#include "helpers/blocks.h"
#include "helpers/bitmap.h"
#include "helpers/slist.h"
#include "helpers/inode.h"
#include "helpers/directory.h"
#include "helpers/utilities.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

/**
 * This will initialize our file system by making the file and
 * mounting it to memory so that we can use it
 * 
 * @param path the path to the file that we are mounting onto.
 */
void storage_init(const char *path) {
  printf("Initializing file system.\n");
  // Initializes the file system
  blocks_init(path);
  // Check if our inode blocks already exist.
  if(bitmap_get(get_blocks_bitmap(), 1) == 0) {
    // We need to allocate the blocks for our storage system
    for(int i = 0; i < NUM_INODE_BLOCKS; ++i) {
      // If the blocks don't exist at the start in order, our file system is corrupted.
      assert(i + INODE_BLOCK_BEGIN == alloc_block());
    }
    // Makes the root "/" directory
    directory_init();
  }
  else {
    // Blocks apparently do exist, ensure all of them are there
    for(int i = 0; i < NUM_INODE_BLOCKS; ++i) {
      // If not all our blocks exist, then we are missing an inode block and our 
      // file system is corrupted.
      assert(bitmap_get(get_blocks_bitmap(), i + INODE_BLOCK_BEGIN));
    }
    // Assert root directory exists
    assert(bitmap_get(get_inode_bitmap(), ROOT_INODE));
    assert(bitmap_get(get_blocks_bitmap(), get_inode(ROOT_INODE)->block));
  }
  
}

/**
 * Gets the information from the inode at the associate path, and
 * places it in the stat struct. If no file exists, returns -ENOENT.
 * 
 * @param path the path to the file to get info from
 * @param st the stat block to fill in info about.
 * 
 * @returns the status if it worked or not.
 */
int storage_stat(const char* path, struct stat* st){
  printf("Looking up info about %s\n", path);
  int inum = tree_lookup(path);
  if(inum == -1) {
    return -ENOENT;
  }
  inode_t* node = get_inode(inum);
  st->st_size = node->size;
  st->st_mode = node->mode;
  st->st_uid = getuid();
  st->st_ino = inum;
  st->st_nlink = node->refs;
  return 0;
}

/**
 * Reads from the file into the buffer. Reads at most size bytes
 * 
 * @param path the path to the file to read from
 * @param buf the buffer to read into.
 * @param size the maximum amount of bytes to read
 * @param offset where in the file to read from.
 * 
 * @returns the status of success.
*/
int storage_read(const char *path, char *buf, size_t size, off_t offset) {
  printf("Reading from %s at %zu for at most %zu bytes.\n", path, offset, size);
  int inum = tree_lookup(path);
  if(inum == -1) {
    return -ENOENT;
  }
  inode_t* node = get_inode(inum);
  // Check that it isn't a directory
  if(node->mode / 010000 == 4) {
    return -EISDIR;
  }
  // Check for read permissions
  if((((node->mode - 010000) / 0100) & 04) == 04) {
    // Check to ensure not trying to look past file
    if(offset > node->size) {
      return -ESPIPE;
    }
    // Ensure we only read to end of file and not past it
    if(size > node->size - offset) {
      size = node->size - offset;// Can only read to end.
    }
    // Copy from file to buffer.
    memcpy(buf, blocks_get_block(node->block) + offset, size);
    // Return how much read.
    return size;
  }
  else {
    return -EACCES;
  }
}
/**
 * Write from buffer into file. Fails if there is too much
 * in the buffer that can't be stored in the file.
 * 
 * @param path of the file to write into
 * @param buf the buffer to write from.
 * @param size the size of the write
 * @param offset the offset in the file to write from.
 * 
 * @returns the status of the write.
*/
int storage_write(const char *path, const char *buf, size_t size, off_t offset) {
  printf("Writing from %s at %zu for %zu bytes.\n", path, offset, size);
  // Find number of blocks required
  int num_blocks = bytes_to_blocks(size + offset);
  // Check if it exists
  int inum = tree_lookup(path);
  if(inum == -1) {
    return -ENOENT;
  }
  // If need more than one block, we can't hold that so error.
  if(num_blocks > 1) {
    return -EFBIG;
  }
  inode_t* node = get_inode(inum);
  // Check that it isn't a directory
  if(node->mode / 010000 == 4) {
    return -EISDIR;
  }
  // Check for write permissions
  if((((node->mode - 010000) / 0100) & 02) == 02) {
    // Already know wont write past end of file or we 
    // would have needed more than 1 block
    memcpy(blocks_get_block(node->block) + offset, buf, size);
    // grow the inode.
    grow_inode(node, offset + size);
    return size;
  }
  else {
    return -EACCES;
  }
}

/**
 * Truncates the file to a given size. Either larger or smaller
 * 
 * @param path the path of the file to truncate
 * @param size the size to truncate to must be between 0 and block size.
 * 
 * @returns the status of the truncate.
*/
int storage_truncate(const char *path, off_t size) {
  printf("Truncating %s to %zu bytes\n", path, size);
  // Check it exists
  int inum = tree_lookup(path);
  if(inum == -1) {
    return -ENOENT;
  }
  // Make sure not truncating to larger than a block
  if(size > BLOCK_SIZE) {
    return -EFBIG;
  }
  else if(size < 0) {
    // Must be a positive integer, can truncate back to 0 to erase everything.
    return -EINVAL;
  }
  inode_t* node = get_inode(inum);
  // Check that it isn't a directory
  if(node->mode / 010000 == 4) {
    return -EISDIR;
  }
  // Check for write permissions
  if((((node->mode - 010000) / 0100) & 02) == 02) {
      // Truncate it
      shrink_inode(node, size);
      // If it was larger set everything to 0.
      if(size > node->size) {
        memset(blocks_get_block(node->block) + node->size, 0, size - node->size);
      }
  }
  else {
    return -EACCES;
  }
}
/**
 * Creates a file or directory at the given path with the given mode.
 * 
 * @param path of the new file to make
 * @param mode mode of the new file to make
 * 
 * @returns the status of creating the file.
*/
int storage_mknod(const char *path, int mode) {
  printf("Making file %s with mode %d\n", path, mode);
  if(tree_lookup(path) != -1) {
    return -EEXIST;
  }
  // Get parent location
  char* parent = get_parent(path);
  printf("Found parent directory %s\n", parent);
  // Find the inum
  int parent_num = tree_lookup(parent);
  printf("Found parent inum %d\n", parent_num);
  // Get childs name
  char child[strlen(path) - strlen(parent) + 1];
  // + 1 to avoid the /
  strcpy(child, path + strlen(parent));
  printf("Found childs name %s\n", child);
  // Free the parent address
  free(parent);
  // If not found error out
  if(parent_num == -1) {
    return -ENOENT;
  }
  // Get actual inode
  inode_t* parent_node = get_inode(parent_num);
  // If its a directory error out
  if(!is_directory(parent_node)) {
    return -ENOTDIR;
  }
  // Check for write permissions
  if((((parent_node->mode - 040000) / 0100) & 02) != 02) {
    return -EACCES;
  }
  // Check for free space
  if(parent_node->size == BLOCK_SIZE) {
    return -ENOSPC;
  }
  // Check thatchild name is short enough
  if(strlen(child) > DIR_NAME_LENGTH) {
    return -EINVAL;
  }
  // Make the new inode for the file.
  int child_num = alloc_inode();
  printf("Child allocated inode %d\n", child_num);
  // Fails if can't make a new one
  if(child_num == -1) {
    return -ENOSPC;
  }
  // Put new inode and child in directory
  directory_put(parent_node, child, child_num);
  // Set up inode to right mode
  inode_t* child_node = get_inode(child_num);
  child_node->size = 0;
  child_node->mode = mode;
  child_node->refs = 1;
  // If its a directory add the base files
  if(mode / 010000 == 4) {
    directory_put(child_node, "..", parent_num);
    directory_put(child_node, ".", child_num);
  }
  return 0;
}

/**
 * Unlinks the file at that path from the directory.
 * 
 * @param path the path to unlink
 * 
 * @returns the status of the unlink
*/ 
int storage_unlink(const char *path) {
  printf("Unlinking %s\n", path);
  char* parent = get_parent(path);
  printf("Found parent directory %s\n", parent);
  int inum = tree_lookup(parent);
  printf("Found parent inum %d\n", inum);
  // Get child
  char child[strlen(path) - strlen(parent) + 1];
  // to ignore the /, + 1
  strcpy(child, path + strlen(parent));
  printf("Found childs name %s\n", child);
  free(parent);
  // Ensure parent exists
  if(inum == -1) {
    return -ENOENT;
  }
  // Get parent inode
  inode_t* node = get_inode(inum);
  // Ensure it is a directory
  if(node->mode / 010000 != 4) {
    return -ENOTDIR;
  }
  // Ensure write perms
  if((((node->mode - 040000) / 0100) & 02) != 02) {
    return -EACCES;
  }
  // Delete it from the directory.
  directory_delete(get_inode(inum), child);
}
/**
 * Links a file from the old "from" path to a new "to" path.
 * From is the source of the file, to is the new file.
 * 
 * @param from the existing file's filepath
 * @param to the new filepath to link to the file
 * 
 * @returns the status of the link.
*/
int storage_link(const char *from, const char *to) {
  printf("Linking from %s to %s", from, to);
  int from_inum = tree_lookup(from);
  char* to_parent = get_parent(to);
  int to_parent_inum = tree_lookup(to_parent);
  // Get childs name
  char child[strlen(to) - strlen(to_parent) + 1];
  // to ignore the /, + 1
  strcpy(child, to + strlen(to_parent));
  free(to_parent);
  // Make sure from exists
  if(from_inum == -1) {
    return -ENOENT;
  }
  // Make sure to doesn't exist
  if(tree_lookup(to) != -1) {
    return -EEXIST;
  }
  // Ensure to's parent exists
  if(to_parent_inum == -1) {
    return -ENOENT;
  }
  // Get parent not
  inode_t* parent_node = get_inode(to_parent_inum);
  // Ensure parent is directory
  if(parent_node->mode / 010000 != 4) {
    return -ENOTDIR;
  }
  // Ensure parent  has space
  if(parent_node->size == BLOCK_SIZE) {
    return -ENOSPC;
  }
  // Ensure write permissions to parent
  if((((parent_node->mode - 040000) / 0100) & 02) != 02) {
    return -EACCES;
  }
  // Add froms inode to the parent of to.
  directory_put(parent_node, child, from_inum);
}
/**
 * Renames a file in the storage system. From is the source of the file
 * and to is the new name for the file
 * 
 * @param from the current filepath the file has
 * @param to the file path to move the file to.
 * 
 * @returns the status of the rename.
*/
int storage_rename(const char *from, const char *to) {
  printf("Renaming %s to %s\n", from, to);
  int rv = storage_link(from, to);
  if(rv < 0) {
    return rv;
  }
  else {
    return storage_unlink(from);
  }
}
/**
 * Removes an entire path from storage. Errors if 
 * they lack permissions to do so or the path given
 * is not a directory
 * 
 * @param path is the path to the directory
 * 
 * @returns status of the rmdir.
*/
int storage_rmdir(const char* path) {
  printf("Removing path at %s\n", path);
  if(path == "/") {
    // You can't delete your whole file system!
    return -EPERM;
  }
  // Get inode and confirm it is a directory
  int inum = tree_lookup(path);
  inode_t* node = get_inode(inum);
  // confirm directory
  if(!is_directory(node)) {
    return -ENOTDIR;
  }
  // confirm empty (only . and ..)
  if(node->size > (2 * SIZE_DIRENT)) {
    return -ENOTEMPTY;
  }
  // Confirm we can edit
  if((((node->mode - 040000) / 0100) & 02) == 02) {
    // Remove it from the parent
    char* parent = get_parent(path);
    // We know parent exists
    inode_t* parent_node = get_inode(tree_lookup(parent));
    char child[strlen(path) - strlen(parent) + 1];
    // Copy over child name
    strcpy(child, path + strlen(parent));
    // Free . and .. in the directory
    directory_delete(node, ".");
    directory_delete(node, "..");
    // Delete directory from parent
    directory_delete(parent_node, child);
    free(parent);

    return 0;
  }
  else {
    return -EACCES;
  }
}


/**
 * Unimplemented as we don't use time.
*/
int storage_set_time(const char *path, const struct timespec ts[2]) {
  return -EAFNOSUPPORT;
}

/**
 * Lists all files in storage at the available path.
 * Fails if the path points to a file and not a directory
 * 
 * @param path path to directory to look up all files from it.
 * 
 * @returns the list of all entries in that directory.
*/
slist_t *storage_list(const char *path) {
  printf("Getting entries from %s\n", path);
  // Get inode at path
  int inum = tree_lookup(path);
  inode_t* node = get_inode(inum);
  // Return error if not a directory
  if(!is_directory(node)) {
    return NULL;
  }
  // Get all entries
  return directory_list(node);
}
