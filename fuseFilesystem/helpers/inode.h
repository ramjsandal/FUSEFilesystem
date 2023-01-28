// Inode manipulation routines.
//
// Feel free to use as inspiration.

// based on cs3650 starter code

#ifndef INODE_H
#define INODE_H

#include "blocks.h"

// Location of root directory inode
#define ROOT_INODE 0

typedef struct inode {
  int refs;  // reference count
  int mode;  // permission & type
  int size;  // bytes
  int block; // single block pointer (if max file size <= 4K)

} inode_t;

// Just to know how large our Inodes are (since that can change)
#define INODE_SIZE sizeof(inode_t)

void print_inode(inode_t *node);
inode_t *get_inode(int inum);
int alloc_inode();
void free_inode(int inum);
void decrement_references(int inum); // Decreases the number of references an inode has 
                                     // and frees it if its out of references
void grow_inode(inode_t *node, int size);
void shrink_inode(inode_t *node, int size);
int inode_get_bnum(inode_t *node, int fbnum);

#endif
