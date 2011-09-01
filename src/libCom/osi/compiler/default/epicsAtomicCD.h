
/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#ifndef epicsAtomicCD_h
#define epicsAtomicCD_h

#if __STDC_VERSION__ >= 199901L || defined ( __cplusplus )
#   define EPICS_ATOMIC_INLINE inline
    /*
     * We have already defined the public interface in epicsAtomic.h
     * so there is nothing more to implement if there isnt an inline 
     * keyword available. Otherwise, if we have an inline keyword
     * we will proceed with an os specific inline implementation.
     */
#   include "epicsAtomicOSD.h"
#endif

#endif /* epicsAtomicCD_h */
