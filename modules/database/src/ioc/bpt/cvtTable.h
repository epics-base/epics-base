/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Breakpoint Tables
 *
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
 */

#ifndef INCcvtTableh
#define INCcvtTableh    1

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Global Routines*/
DBCORE_API long cvtEngToRawBpt(
    double *pval,short linr,short init,void **ppbrk,short *plbrk);

DBCORE_API long cvtRawToEngBpt(
    double *pval,short linr,short init,void **ppbrk, short *plbrk);

#ifdef __cplusplus
}
#endif

#endif
