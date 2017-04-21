/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_NEWDEL_H
#define GDD_NEWDEL_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 */

// this file if formatted with tab stop = 4

#include <stdlib.h>
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsThread.h"

// Avoid using templates at the cost of very poor readability.
// This forces the user to have a static data member named "gddNewDel_freelist"

// To use this stuff:
//
// ** In class description header file:
//  class myClass
//  {
//  public:
//      gdd_NEWDEL_FUNC(address_to_be_used_for_freelist_next_pointer)
//  private:
//      gdd_NEWDEL_DATA(myClass)
//  };
//
// ** In source file where functions for class are written:
//  gdd_NEWDEL_STAT(myClass)
//  gdd_NEWDEL_DEL(myClass)
//  gdd_NEWDEL_NEW(myClass)

#define gdd_CHUNK_NUM 20
#define gdd_CHUNK(mine) (gdd_CHUNK_NUM*sizeof(mine))

void gddGlobalCleanupAdd ( void * pBuf );

// private data to add to a class
#define gdd_NEWDEL_DATA \
    static char* newdel_freelist; \
    static epicsMutex *pNewdel_lock; \
    static epicsThreadOnceId once;

// public interface for the new/delete stuff
// user gives this macro the address they want to use for the next pointer
#define gdd_NEWDEL_FUNC(fld) \
    void* operator new(size_t); \
    void operator delete(void*); \
    char* newdel_next(void) { char* pfld = (char *)&fld; \
        char** x = (char**)pfld; return *x; } \
    void newdel_setNext(char* n) { char* pfld = (char *)&fld; \
        char** x=(char**)pfld; *x=n; } \
    static void gddNewDelInit (void) { pNewdel_lock = newEpicsMutex; }


// declaration of the static variable for the free list
#define gdd_NEWDEL_STAT(clas) \
    char* clas::newdel_freelist=NULL; \
    epicsMutex * clas::pNewdel_lock = NULL; \
    epicsThreadOnceId clas::once = EPICS_THREAD_ONCE_INIT;

// code for the delete function
#define gdd_NEWDEL_DEL(clas) \
 void clas::operator delete(void* v) { \
    clas* dn = (clas*)v; \
    if(dn->newdel_next()==(char*)(-1)) free((char*)v); \
    else { \
        epicsGuard < epicsMutex > guard ( *clas::pNewdel_lock ); \
        dn->newdel_setNext(clas::newdel_freelist); \
        clas::newdel_freelist=(char*)dn; \
    } \
 }

// following function assumes that reading/writing address is atomic

// code for the new function
#define gdd_NEWDEL_NEW(clas) \
 extern "C" { void clas##_gddNewDelInit ( void * ) { \
    clas::gddNewDelInit(); } } \
 void* clas::operator new(size_t size) { \
    int tot; \
    clas *nn,*dn; \
    epicsThreadOnce ( &once, clas##_gddNewDelInit, 0 ); \
    epicsGuard < epicsMutex > guard ( *clas::pNewdel_lock ); \
    if(!clas::newdel_freelist) { \
        tot=gdd_CHUNK_NUM; \
        nn=(clas*)malloc(gdd_CHUNK(clas)); \
        gddGlobalCleanupAdd (nn); \
        for(dn=nn;--tot;dn++) dn->newdel_setNext((char*)(dn+1)); \
        (dn)->newdel_setNext(clas::newdel_freelist); \
        clas::newdel_freelist=(char*)nn; \
    } \
    if(size==sizeof(clas)) { \
        { \
            dn=(clas*)clas::newdel_freelist; \
            clas::newdel_freelist=((clas*)clas::newdel_freelist)->newdel_next(); \
        } \
        dn->newdel_setNext(NULL); \
    } else { \
        dn=(clas*)malloc(size); \
        dn->newdel_setNext((char*)(-1)); \
    } \
    return (void*)dn; \
 }

#endif

