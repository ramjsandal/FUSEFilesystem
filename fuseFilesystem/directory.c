#include "helpers/directory.h"
#include "helpers/inode.h"
#include "helpers/bitmap.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/**
 * This file is the setup for the entire directory tree the system runs on.
*/

/**
 * Initializes the root directory.
*/
void directory_init() {
    // Assert it isn't allocated yet
    assert(bitmap_get(get_inode_bitmap(), ROOT_INODE) == 0);
    printf("Making root directory\n");
    // Initialize root directory
    bitmap_put(get_inode_bitmap(), ROOT_INODE, 1);
    inode_t* root = get_inode(ROOT_INODE);
    // Special initialization
    root->block = alloc_block();
    // Ensure it allocated properly
    assert(root->block != -1);
    root->size = 0;
    root->refs = 1;
    root->mode = 040755;
    // Put in basic information
    directory_put(root, ".", ROOT_INODE);
    directory_put(root, "..", ROOT_INODE);
}

/**
 * Looks for the given name on this directory and returns the inode
 * number associated with it. Returns -1 if not found or if not directory.
 * 
 * @param dd the inode to the directory.
 * @param name the name to find in the directory
 * 
 * @returns the inode of the associate name in teh directory.
*/
int directory_lookup(inode_t *dd, const char *name) {
    // Confirm it is a directory.
    if(!is_directory(dd)) {
        return -1;
    }
    printf("Searching for %s\n", name);
    dirent_t* entry = blocks_get_block(dd->block);
    // Loop through all entries to find one that matches
    for(int i = 0; i < dd->size/SIZE_DIRENT; ++i) {
        printf("Looking at directory entry %s\n", (entry + i)->name);
        if(strcmp((entry + i)->name, name) == 0) {
            printf("Found %s at %d\n", name, (entry + i)->inum);
            return (entry + i)->inum;
        }
    }
    return -1;
}

/**
 * Finds the inode number at the given path, or -1 if there isn't one.
 * 
 * @param path: the absolute path to look for
 * 
 * @returns the inode at that path or -1 if there isn't one.
 */
int tree_lookup(const char *path) {
    printf("Looking for %s\n", path);
    // Ignore the starting /
    slist_t* jumps = s_explode(path + 1, '/');
    slist_t* next = jumps;
    int src = ROOT_INODE;
    while(next != NULL && src != -1) {
        printf("Looking for: %s\n", next->data);
        src = directory_lookup(get_inode(src), next->data);
        next = next->next;
    }
    s_free(jumps);
    printf("Found at %d\n", src);
    return src;
}
/**
 * Adds a new inode to the directory under the given name.
 * Also increment references in inum by 1.
 * 
 * @param dd the directory inode to add to
 * @param name the name of the new file to add to the directory
 * @param inum the inum of the file to add to directory.
*/
int directory_put(inode_t *dd, const char *name, int inum) {
    // We can add to it
    printf("Putting %s, %d\n", name, inum);
    if(dd->size + SIZE_DIRENT > BLOCK_SIZE) {
        return -ENOSPC;
    }
    dirent_t* new = blocks_get_block(dd->block) + dd->size;
    strcpy(new->name, name);
    new->inum = inum;
    get_inode(new->inum)->refs += 1;
    grow_inode(dd, dd->size + SIZE_DIRENT);
    return 0;
}

/**
 * Removes the reference with the given name from the directory.
 * 
 * @param dd the directory inode to delete from
 * @param name the file name to delete from.
*/
int directory_delete(inode_t *dd, const char *name) {
    printf("Removing %s\n", name);
    dirent_t* entry = blocks_get_block(dd->block);
    // Loop through all entries to find one that matches
    for(int i = 0; i < dd->size/SIZE_DIRENT; ++i) {
        if(strcmp((entry + i)->name, name) == 0) {
            decrement_references((entry + i)->inum);
            memcpy(entry + i, entry + i + 1, dd->size - ((i + 1) * SIZE_DIRENT));
            shrink_inode(dd, dd->size - SIZE_DIRENT);
            return 0;
        }
    }
    return -ENOENT;
}

/**
 * Lists all the entries in this directory.
 * 
 * @param dd is the directory inode to list entries from.
*/
slist_t *directory_list(inode_t* dd) {
    printf("Getting entries\n");
    dirent_t* entry = blocks_get_block(dd->block);
    slist_t* entries = NULL;
    // Loop through all entries
    for(int i = 0; i < dd->size/SIZE_DIRENT; ++i) {
        entries = s_cons((entry + i)->name, entries);
    }
    return entries;
}

/**
 * Prints what the directory looks like.
 * 
 * @param dd the directory inode to print info about.
*/
void print_directory(inode_t *dd) {
    print_inode(dd);
    slist_t* entries = directory_list(dd);
    slist_t* next = entries;
    while(next != NULL) {
        printf("%s\n", next->data);
        next = next->next;
    }
    s_free(entries);
}

int is_directory(inode_t* node) {
    return node->mode / 010000 == 4;
}
