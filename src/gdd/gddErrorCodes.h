/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_ERROR_CODES_H
#define GDD_ERROR_CODES_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.3  1997/04/10 19:59:26  jhill
 * api changes
 *
 * Revision 1.2  1996/08/27 13:05:10  jbk
 * final repairs to string functions, put() functions, and error code printing
 *
 * Revision 1.1  1996/06/25 19:11:43  jbk
 * new in EPICS base
 *
 *
 * *Revision 1.2  1996/06/13 21:32:00  jbk
 * *Various fixes and correction - including ref_cnt change to unsigned short
 * *Revision 1.1  1996/05/31 13:15:30  jbk
 * *add new stuff
 *
 */

#include "shareLib.h"

/*
   gdd.cc contains a table (gddErrorMessages) that has all the text
   strings for each of the error codes
*/

typedef long gddStatus;

#define gddErrorTypeMismatch	-1
#define gddErrorNotAllowed		-2
#define gddErrorAlreadyDefined	-3
#define gddErrorNewFailed		-4
#define gddErrorOutOfBounds		-5
#define gddErrorAtLimit			-6
#define gddErrorNotDefined		-7
#define gddErrorNotSupported	-8
#define gddErrorOverflow		-9
#define gddErrorUnderflow		-10

epicsShareExtern char* gddErrorMessages[];

#define gddPrintError(x) \
	fprintf(stderr,"gdd Error: %s\n",gddErrorMessages[x*(-1)]);

#define gddPrintErrorWithMessage(msg,x) \
	fprintf(stderr,"gdd Error: %s (%s)\n",gddErrorMessages[x*(-1)],msg);

#define gddGetErrorMessage(x) gddErrorMessages[x*(-1)]

#ifdef GDDAUTOPRINT
#define gddAutoPrint(s,x) if(x) gddPrintErrorWithMessage(s,x)
#else
#define gddAutoPrint(s,x) ;
#endif

#endif
