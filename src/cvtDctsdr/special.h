/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* special.h */
/* share/epicsH $Id$ */

/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCspecialh
#define INCspecialh 1
/* types 1-99 are global. Record specific must start with 100 */
#define SPC_NOMOD	1	/*Field must not be modified			*/
#define SPC_DBADDR	2	/*db_name_to_addr must call cvt_dbaddr	*/
#define SPC_SCAN	3	/*A scan related field is being changed		*/
#define SPC_HDWLINK	4	/*Special Hardware Link		*/
#define SPC_ALARMACK	5	/*Special Alarm Acknowledgement*/
#define SPC_AS		6	/* Access Security*/
/* useful when record support must be notified of a field changing value*/
#define SPC_MOD		100
/* used by all records that support a reset field		*/
#define SPC_RESET	101	/*The res field is being modified*/
/* Specific to conversion (Currently only ai */
#define SPC_LINCONV	102	/*A linear conversion field is being changed*/
/* Specific to calculation records */
#define SPC_CALC	103	/*The CALC field is being changed*/
#endif
