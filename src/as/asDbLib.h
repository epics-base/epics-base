/* share/epicsH/dbAsLib.h	*/
/*  $Id$ */
/* Author:  Marty Kraimer Date:    02-23-94*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 * Modification Log:
 * -----------------
 * .01  02-23-94	mrk	Initial Implementation
 */

#ifndef INCdbAsLibh
#define INCdbAsLibh
#include <asLib.h>
#include <callback.h>

typedef struct {
    CALLBACK	callback;
    long	status;
} ASDBCALLBACK;

int asSetFilename(char *acf);
int asSetSubstitutions(char *substitutions);
int asInitAsyn(ASDBCALLBACK *pcallback);
int asDbGetAsl( void *paddr);
ASMEMBERPVT  asDbGetMemberPvt( void *paddr);
int asdbdump( void);
int aspuag(char *uagname);
int asphag(char *hagname);
int asprules(char *asgname);
int aspmem(char *asgname,int clients);
void asCaStart(void);
void asCaStop(void);

#endif /*INCdbAsLibh*/
