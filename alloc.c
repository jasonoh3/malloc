/**
 * Malloc
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct _metadata_t {
  size_t size;
  unsigned char is_used;
  void* next;
  void* prev;
} metadata_t;

void* start_of_heap = NULL;
void* head_free = NULL;
void* tail_free = NULL;

// void* == metadata*
void add_node(void* ptr) {
  metadata_t* data = ptr;
  data->prev = tail_free;
  if (head_free && tail_free) {
    metadata_t* curr_free = tail_free;
    curr_free->next = ptr;
    tail_free = ptr;
  } else { // No free blocks
    head_free = ptr;
    tail_free = ptr;
  }
}

// void* == metadata*
size_t delete_node(void* ptr) {
  if (!head_free) return 1; // failed to delete node because there is no nodes in free_list
  metadata_t* curr = head_free;
  metadata_t* prev = curr->prev;
  while (curr) {
    if (curr == ptr) {
      if (head_free == tail_free) { // only one block
        curr->next = NULL;
        curr->prev = NULL;
        head_free = NULL;
        tail_free = NULL;
        return 0;
      }
      if (prev && tail_free == ptr) tail_free = prev; // set tail_free to previous if ptr is the last node to be deleted
      head_free == ptr ? (head_free = curr->next) : (prev->next = curr->next);
      if (curr->next) {
        metadata_t* temp = curr->next;
        temp->prev = prev;
      }
      return 0; // successfully deleted node
    }
    prev = curr;
    curr = curr->next;
  }
  return 1; // failed to delete node as it does not exist or could not find it
}

// void* == metadata*
void edit_node(void* old_block, void* new_block) {
  metadata_t* block = old_block;
  metadata_t* new_block_tran = new_block;
  metadata_t* curr_free = head_free;
  while (curr_free) {
    if (curr_free == block) {
      if ((void*) curr_free == head_free) head_free = new_block;
      if ((void*) curr_free == tail_free) tail_free = new_block;
      if (curr_free->prev) {
        metadata_t* prev = curr_free->prev;
        prev->next = new_block;
        new_block_tran->prev = prev;
      }
      if (curr_free->next) {
        metadata_t* next = curr_free->next;
        next->prev = new_block;
        new_block_tran->next = next;
      }
      return;
    }
    curr_free = curr_free->next;
  }
}

// void* == metadata*
void* split_block(void* ptr, size_t size) {
  metadata_t* block = ptr;
  size_t block_size = block->size;

  block->size = size;
  block->is_used = 1;

  // split into two chunks
  metadata_t* new_block = (void*) block + block->size + sizeof(metadata_t);
  new_block->size = block_size - size - sizeof(metadata_t);
  new_block->is_used = 0;
  new_block->next = NULL;
  new_block->prev = NULL;
  edit_node(block, new_block);

  return (void*) ptr + sizeof(metadata_t);
}

// void* == metadata*
size_t coalesce_blocks(void* block) {
  if (!head_free) return 0;
  metadata_t* curMeta = start_of_heap;
  void* endOfHeap = sbrk(0);
  while ((void*) curMeta < endOfHeap) {
    metadata_t* tempMeta = curMeta;
    curMeta = (void*) tempMeta + tempMeta->size + sizeof(metadata_t);
    if ((void*) curMeta > endOfHeap) return 0;
    if (tempMeta->is_used == 0 && curMeta->is_used == 0) {
      tempMeta->size += curMeta->size + sizeof(metadata_t);
      // If the new block is the new face of the TOTAL block,
      // Then, we must remove the old block from the free list
      if (tempMeta == block) return delete_node(curMeta);
      if (curMeta == block) {
        // coalesce the third free block if it exists
        metadata_t* third_block = (void*) curMeta + curMeta->size + sizeof(metadata_t);
        metadata_t* curr_free = head_free;
        while (curr_free) {
          if (curr_free == third_block) {
            tempMeta->size += third_block->size + sizeof(metadata_t);
            delete_node(third_block);
          }
          curr_free = curr_free->next;
        }
        // 0: add new block to the end of free list b/c new block is the face of the TOTAL block
        // 1: do NOT add new block to the end of free list b/c old block is still the face of the TOTAL block
        return delete_node(curMeta);
      }
    }
  }
  return 0; // add new block to the end of free list b/c failed coalesce
}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void* calloc(size_t num, size_t size) {
  size_t mem_block_size = num * size;
  void* ptr = malloc(mem_block_size);
  memset(ptr, '\x00', mem_block_size);
  return ptr;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */

void* malloc(size_t size) {
  if (!start_of_heap) start_of_heap = sbrk(0);
  metadata_t* curr = head_free;

  while (curr) {
    if (curr->size == size) {
      delete_node(curr);
      curr->is_used = 1;
      return (void*) curr + sizeof(metadata_t);
    }
    if (curr->size > size + sizeof(metadata_t)) return split_block(curr, size);
    curr = curr->next;
  }

  metadata_t* meta = sbrk(sizeof(metadata_t));
  meta->size = size;
  meta->is_used = 1;
  meta->next = NULL;
  meta->prev = NULL;

  return sbrk(size);
}


/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
  metadata_t* meta = ptr - sizeof(metadata_t);
  meta->is_used = 0;

  // Add new free block/node
  if (!head_free || coalesce_blocks(meta) == 0) add_node(meta);
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void* realloc(void *ptr, size_t size) {
  if (!ptr) return malloc(size);
  if (size == 0 && ptr) {
    free(ptr);
    return NULL;
  }

  metadata_t* block = (void*) ptr - sizeof(metadata_t);
  if (block->size >= size) return ptr;
  
  // transport to new location
  void* new_ptr = malloc(size);
  memcpy(new_ptr, ptr, block->size);
  free(ptr);
  return new_ptr;
}