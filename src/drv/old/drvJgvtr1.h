/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvJgvtr1.h */
/* base/src/drv $Id$ */
/*
 *      Author:      Jeff Hill
 *      Date:        5-89
 */



typedef long    jgvtr1Stat;

#define JGVTR1_SUCCESS  0

jgvtr1Stat jgvtr1_driver(
unsigned        card,
void            (*pcbroutine)(void *, unsigned, uint16_t *),
void            *parg
);

