#ifndef DBMF_H
#define DBMF_H

/*
 * Author: Jim Kowalkowski
 * Date:   4/97
 *
 * Simple library to manage reusing buffers of similar sizes using
 * malloc/free type style calls.
 *
 * void* dbmfInit(MAX_BUFFER_SIZE,REQUIRED_BUFFER_ALIGNMENT,INITIAL_POOL_SIZE);
 *
 * void* dbmfMalloc(handle,byte_count);
 * void* dbmfFree(handle,buffer_pointer);
 *
 * dbmfInit() returns a handle to a buffer pool.
 * Use dbmfMalloc() and dbmfFree() to get buffers from it.  The buffer pool
 * will give you managed buffers for sizes of MAX_BUFFER_SIZE and less.
 * Sizes requested which are larger than MAX_BUFFER_SIZE will just be
 * allocated using normal malloc() call.  A buffer gotten with dbmfMalloc()
 * will be aligned to REQUIRED_BUFFER_ALIGNMENT.  The buffer pool will
 * grow by INITIAL_POOL_SIZE each time it runs out of buffers.
 *
 * $Id$
 * $Log$
 *
 */

void* dbmfInit(size_t size, size_t alignment, int init_num_free_list_items);
void* dbmfMalloc(void* handle,size_t bytes);
void  dbmfFree(void* handle,void* bytes);

#endif
