#include "helpers/utilities.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * Appends two paths in a new path.
 * Returned value needs to be freed.
 * 
 * @param path1 the front of the path to append
 * @param path2 the back of the path to append
 * 
 * @returns a new path that is the combined path of both, must be freed.
 */
char* append(const char* path1, const char* path2){
  assert(strlen(path1) > 0);
  assert(strlen(path2) > 0);
  // Extra space added for /
  char* new = malloc(strlen(path1) + strlen(path2) + 2);
  for(int i = 0; *(path1 + i) != 0; ++i) {
    new[i] = *(path1 + i);
  }
  int offset = strlen(path1);
  // Add the slash to the path if needed
  if(new[offset - 1] != '/') {
    new[offset] = '/';
    ++offset;
  }
  for(int i = 0; *(path2 + i) != 0; ++i) {
    new[offset + i] = *(path2 + i);
  }
  new[offset + strlen(path2)] = 0;
  return new; 
}

/**
 * Gets the path to the parent of the described path.
 * Returned path needs to be freed.
 * 
 * @param path the path to get the parent from
 * 
 * @returns a new path representing the parent to the original path.
 *          This has been malloc'd and needs to be freed.
*/
char* get_parent(const char* path) {
    int child_length = 0;
    // Loop backwards until first "/" is found.
    for(int i = strlen(path) - 1; *(path + i) != '/'; --i) {
        ++child_length;
    }

    // Copy path over to new locations
    char* parent = malloc(strlen(path) + 1);
    strcpy(parent, path);
    // Replace the last "/" before the child with a terminator.
    *(parent + strlen(path) - child_length) = 0;
    return parent;
}
