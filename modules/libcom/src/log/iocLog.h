/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file iocLog.h
 * \brief Exports the deﬁnitions for the iocLogClient.
 * \version 1.5.2.1
 * \author Jeffrey O. Hill
 * \date 08-07-91
 * 
 * For backward compatibility with older versions of the logClient library some
 * deﬁnitions are exported for the standard iocLogClient for error logging 
 */

#ifndef INCiocLogh
#define INCiocLogh 1
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ioc log client interface
 */
/** \brief Enable/disable the messages from being sent to the server */
LIBCOM_API extern int iocLogDisable;
/** \brief Calles the iocLogClientInit.*/
LIBCOM_API int epicsStdCall iocLogInit (void);
/** \brief Calles logClientShow.*/
LIBCOM_API void epicsStdCall iocLogShow (unsigned level);
/** \brief Calles logClientFlush.*/
LIBCOM_API void epicsStdCall iocLogFlush (void);

#ifdef __cplusplus
}
#endif

#endif /*INCiocLogh*/
