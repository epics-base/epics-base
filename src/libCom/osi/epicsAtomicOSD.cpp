
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
 
 #include "epicsAtomic.h"
 
 /*
  * OSD_ATOMIC_INLINE
  * defined: define atomic functions in osdAtomic.h
  * undefined: dont define atomic functions in osdAtomic.h
  *
  * OSD_ATOMIC_INLINE is typically set by the compiler specific file cdAtomic.h
  *
  * when we discover here that there is no implementation, and we guess that
  * there isnt an inline keyword in the compiler then we define OSD_ATOMIC_INLINE
  * empty and include osdAtomic.h to force an out of line implementation
  *
  * My first inclination was to place only one file of this type under os/default, 
  * but then this source file always includes default/osdAtomic.h, which isnt correct 
  * if a more specific os/xxx/osdAtromic.h exists, and so rather than duplicate an 
  * identical file in all of the os directories (that will get rediculous if there 
  * is ever a posix interface for the atomics) I moved it here to the osi directory. 
  * Another option would be to not make tthe current location be the first search 
  * in the gnu make VPATH, but I am somewhat nervous about changing something that
  * fundamental in the build system.
  *
  * this behavior will work most of the time by defualt but it can always be 
  * replaced by an os specific implementation, which might be required if
  * some of the functions are inline and some are not
  */
 #ifndef OSD_ATOMIC_INLINE
 #  include "osdAtomic.h"
 #endif
 
