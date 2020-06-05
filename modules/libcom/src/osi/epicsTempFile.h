/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsTempFile.h
 * \brief OS-independent way to create temporary files.
 **/

#ifndef INC_epicsTempFile_H
#define INC_epicsTempFile_H

#include <stdio.h>

#include "libComAPI.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 *  \brief Create and open a temporary file.
 *  \return NULL or a FILE pointer to a temporary file.
 **/
LIBCOM_API FILE * epicsStdCall epicsTempFile(void);

#ifdef  __cplusplus
}
#endif

#endif /* INC_epicsTempFile_H */
