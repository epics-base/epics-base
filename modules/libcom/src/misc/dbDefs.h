/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INC_dbDefs_H
#define INC_dbDefs_H

#include <stddef.h>

#ifdef TRUE
#   undef TRUE
#endif
#define TRUE 1

#ifdef FALSE
#   undef FALSE
#endif
#define FALSE 0

/* deprecated, use static */
#ifndef LOCAL
#   define LOCAL static
#endif

/* number of elements in an array */
#ifndef NELEMENTS
#   define NELEMENTS(array) (sizeof (array) / sizeof ((array) [0]))
#endif

/* byte offset of member in structure - deprecated, use offsetof */
#ifndef OFFSET
#   define OFFSET(structure, member) offsetof(structure, member)
#endif

/* Subtract member byte offset, returning pointer to parent object */
#ifndef CONTAINER
# ifdef __GNUC__
#   define CONTAINER(ptr, structure, member) ({                     \
        const __typeof(((structure*)0)->member) *_ptr = (ptr);      \
        (structure*)((char*)_ptr - offsetof(structure, member));    \
    })
# else
#   define CONTAINER(ptr, structure, member) \
        ((structure*)((char*)(ptr) - offsetof(structure, member)))
# endif
#endif

/*Process Variable Name Size */
/* PVNAME_STRINGSZ includes the nil terminator */
#define PVNAME_STRINGSZ 61
#define PVNAME_SZ (PVNAME_STRINGSZ - 1)

/* Buffer size for the string representation of a DBF_*LINK field */
#define PVLINK_STRINGSZ 1024

/* dbAccess enums/menus can have up to this many choices */
#define DB_MAX_CHOICES 30

#endif /* INC_dbDefs_H */
