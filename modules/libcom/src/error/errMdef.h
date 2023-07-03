/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file errMdef.h
 * \brief Error Handling definitions.
 * \author Marty Kraimer
 * \date 6-1-90
 * 
 * Module numbers start above 500 for compatibility with vxWorks errnoLib
 * FIXME: M_xxx values could be declared as integer variables and set
 * at runtime from registration routines; the S_xxx definitions would
 * still work with that change, with careful initialization.
 */

#ifndef INC_errMdef_H
#define INC_errMdef_H

#define RTN_SUCCESS(STATUS) ((STATUS)==0)

/* libCom */
#define M_asLib         (501 << 16) /**< \brief Access Security */
#define M_bucket        (502 << 16) /**< \brief Bucket Hash */
#define M_devLib        (503 << 16) /**< \brief Hardware RegisterAccess */
#define M_stdlib        (504 << 16) /**< \brief EPICS Standard library */
#define M_pool          (505 << 16) /**< \brief Thread pool */
#define M_time          (506 << 16) /**< \brief epicsTime */

/* ioc */
#define M_dbAccess      (511 << 16) /**< \brief Database Access Routines */
#define M_dbLib         (512 << 16) /**< \brief Static Database Access */
#define M_drvSup        (513 << 16) /**< \brief Driver Support */
#define M_devSup        (514 << 16) /**< \brief Device Support */
#define M_recSup        (515 << 16) /**< \brief Record Support */

/* cas */
#define M_cas           (521 << 16) /**< \brief CA server */
#define M_gddFuncTbl    (522 << 16) /**< \brief gdd jump table */
#define M_casApp        (523 << 16) /**< \brief CA server application */

#endif /*INC_errMdef_H*/
