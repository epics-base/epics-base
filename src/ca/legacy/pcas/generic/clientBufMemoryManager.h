/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef clientBufMemoryManagerh
#define clientBufMemoryManagerh

#include <limits.h>

typedef unsigned bufSizeT;
static const unsigned bufSizeT_MAX = UINT_MAX;

struct casBufferParm {
    char * pBuf;
    bufSizeT bufSize;
};

class clientBufMemoryManager {
public:
    clientBufMemoryManager();
    ~clientBufMemoryManager();

    //! @throws std::bad_alloc on failure
    casBufferParm allocate ( bufSizeT newMinSize );
    void release ( char * pBuf, bufSizeT bufSize );
private:

    void * smallBufFreeList;
};

#endif // clientBufMemoryManagerh

