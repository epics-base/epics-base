/*
 * Author: Jim Kowalkowski and Marty Kraimer
 * Date:   4/97
 *
 * A library to manage storage that is allocated and then freed.
 */
#ifndef DBMF_H
#define DBMF_H

int   dbmfInit(size_t size, int chunkItems);
void *dbmfMalloc(size_t bytes);
void  dbmfFree(void* bytes);
void  dbmfFreeChunks(void);
int   dbmfShow(int level);
#include "shareLib.h"

/* Rules:
 * 1) Size is always made a multiple of 8.
 * 2) if dbmfInit is not called before one of the other routines then it
 *    is automatically called with size=64 and initial_items=10
 * 3) These routines should only be used to allocate storage that will
 *    shortly thereafter be freed. 
 * 4) dbmfFreeChunks can only free chunks that contain only free items
*/

#endif
