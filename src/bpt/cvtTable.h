/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 * Breakpoint Tables
 *
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
 *
 */

#ifndef INCcvtTableh
#define INCcvtTableh	1

/* Global Routines*/
long cvtEngToRawBpt(double *pval,short linr,short init,
     void **ppbrk,short *plbrk);

long cvtRawToEngBpt(double *pval,short linr,short init,
     void **ppbrk, short *plbrk);

#endif
