#include "helpers/inode.h"
#include "helpers/storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "helpers/blocks.h"
#include "helpers/bitmap.h"

/**
 * Implementation notes:
 * storage.c will be in charge of allocating the blocks for inodes before 
 * requesting any inodes. They should be the first NUM_INODE_BLOCKS blocks
 * in the storage system (besides the reserved one at slot 0)
 * at INODE_BLOCK_BEGIN.
 */

/**
 * Prints the inode to stdout.
 * 
 * @param node the node to print info about.
 */
void print_inode(inode_t* node) {
  printf("Size: %d\n", node->size);
  printf("References: %d\n", node->refs);
  printf("Mode: %d\n", node->mode);
  printf("Block: %d\n", node->block);
}

/**
 * Gets the inode at that inode index.
 * 
 * @param inum is the number of the inode to get. 
 */
inode_t* get_inode(int inum) {
  assert(inum >= 0);
  printf("Getting inode at %d\n", inum);
  // Gets the inode bitmap
  void* inode_map = get_inode_bitmap();
  // Check if the inode exists
  assert(bitmap_get(inode_map, inum));
  // It does exist, get the right block and offset
  int block_offset = inum / INODES_PER_BLOCK;
  int inode_offset = inum % INODES_PER_BLOCK;
  printf("Found in block %d at entry %d\n", INODE_BLOCK_BEGIN + block_offset, inode_offset);
  void* block = blocks_get_block(INODE_BLOCK_BEGIN + block_offset);
  // Get the inode from that block
  return (inode_t*)(block) + inode_offset;
}

/**
 * Allocates a new inode and returns its index. Returns -1
 * if no free spots are available, or if we can't allocate a block.
 * Caller is expected to set up inode properly.
 * 
 * @returns the inum of the allocated inode
*/
int alloc_inode() {
  // Find next open index
  for(int i = 0; i < BLOCK_COUNT; ++i) {
    if(bitmap_get(get_inode_bitmap(), i) == 0) {
      int block_num = alloc_block();
      if(block_num == -1) {
        return -1;
      }
      else {
        bitmap_put(get_inode_bitmap(), i, 1);
        get_inode(i)->block = block_num;
        printf("Allocating inode: %d\n", i);
        return i;
      }
    }
  }
  // Could not allocate.
  return -1;
}

/**
 * Frees an inode and sets it as unoccupied. Always frees its
 * associated block.
 * 
 * @param inum the inode number to free.
*/
void free_inode(int inum) {
  free_block(get_inode(inum)->block);
  bitmap_put(get_inode_bitmap(), inum, 0);
}

/**
 * Reduces the number of references an inode has by 1.
 * If it is now 0, frees the inode.
 * 
 * @param inum the inode number to decrement references from.
*/
void decrement_references(int inum) {
  inode_t* node = get_inode(inum);
  node->refs -= 1;
  if(node->refs == 0) {
    free_inode(inum);
  }
}
/**
 * Grows the inode to the desired size.
 * Fails if size is smaller than current size or bigger than page.
 * 
 * @param node the node to grow 
 * @param size the size to grow to.
*/
void grow_inode(inode_t *node, int size) {
  assert(size >= node->size);
  assert(size <= BLOCK_SIZE);
  node->size = size;
  printf("Updated size to %d\n", node->size);
}
/**
 * Shrinks inode to the desired size.
 * Fails if size is bigger than current size or smaller than 0.
 * 
* @param node the node to shrink 
 * @param size the size to shrink to.
*/
void shrink_inode(inode_t *node, int size) {
  assert(size <= node->size);
  assert(size >= 0);
  node->size = size;
  printf("Updated size to %d\n", node->size);
}

/**
 * For compatibility reasons, this is made in case of larger files in the future.
 * For now should just return the only block and be used with 0.
 * 
 * @param node the node to get the block from
 * @param fbnum the block to get from the node.
 * 
 * @returns the block number requested.
*/
int inode_get_bnum(inode_t *node, int fbnum) {
  assert(fbnum == 0);
  return node->block;
}

