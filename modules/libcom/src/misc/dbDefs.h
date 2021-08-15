/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file dbDefs.h
 * \author Marty Kraimer
 *
 * \brief Miscellaneous macro definitions.
 *
 * This file defines several miscellaneous macros.
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

/** \brief Deprecated synonym for \c static */
#ifndef LOCAL
#   define LOCAL static
#endif

/** \brief Number of elements in array */
#ifndef NELEMENTS
#   define NELEMENTS(array) (sizeof (array) / sizeof ((array) [0]))
#endif

/** \brief Deprecated synonym for \c offsetof */
#ifndef OFFSET
#   define OFFSET(structure, member) offsetof(structure, member)
#endif

/** \brief Find parent object from a member pointer
 *
 * Subtracts the byte offset of the member in the structure from the
 * pointer to the member itself, giving a pointer to parent structure.
 * \param ptr Pointer to a member data field of a structure
 * \param structure Type name of the parent structure
 * \param member Field name of the data member
 * \return Pointer to the parent structure
 * \note Both GCC and Clang will type-check this macro.
 */
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

/** \brief Size of a record name including the nil terminator */
#define PVNAME_STRINGSZ 61
/** \brief Size of a record name without the nil terminator */
#define PVNAME_SZ (PVNAME_STRINGSZ - 1)

/** \brief Buffer size for the string representation of a DBF_*LINK field
 */
#define PVLINK_STRINGSZ 1024

/** \brief dbAccess enums/menus can have up to this many choices */
#define DB_MAX_CHOICES 30

#endif /* INC_dbDefs_H */
