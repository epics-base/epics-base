/* $Id$ */
  
/* cvtBpt.c - Convert using breakpoint table
 *
 *      Author:          Marty Kraimer
 *      Date:            04OCT95
 *	This is adaptation of old bldCvtTable 
 *
 *     Experimental Physics and Industrial Control System (EPICS)
 *
 *     Copyright 1991, the Regents of the University of California,
 *     and the University of Chicago Board of Governors.
 *
 *     This software was produced under  U.S. Government contracts:
 *     (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *     and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *     Initial development by:
 *          The Controls and Automation Group (AT-8)
 *          Ground Test Accelerator
 *          Accelerator Technology Division
 *          Los Alamos National Laboratory
 *
 *     Co-developed with
 *          The Controls and Computing Group
 *          Accelerator Systems Division
 *          Advanced Photon Source
 *          Argonne National Laboratory
 *
 *
 * Modification Log:
 * -----------------
 * 01	04OCT95	mrk	Taken from old bldCvtTable
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ellLib.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "epicsPrint.h"

#define epicsExportSharedSymbols
#include "cvtTable.h"

epicsShareExtern struct dbBase *pdbbase;



static brkTable *findBrkTable(short linr)
{ 
    dbMenu	*pdbMenu;

    pdbMenu = dbFindMenu(pdbbase,"menuConvert");
    if(!pdbMenu) {
	epicsPrintf("findBrkTable: menuConvert does not exist\n");
	return(0);
    }
    if(linr<0 || linr>=pdbMenu->nChoice) {
	epicsPrintf("findBrkTable linr %d  but menuConvert has %d choices\n",
	    linr,pdbMenu->nChoice);
	return(0);
    }
    return(dbFindBrkTable(pdbbase,pdbMenu->papChoiceValue[linr]));
}

long epicsShareAPI cvtRawToEngBpt(double *pval,short linr,short init, void **ppbrk,
	short *plbrk)
{ 
    double	val=*pval;
    long	status=0;
    brkTable	*pbrkTable;
    brkInt	*pInt;     
    brkInt	*pnxtInt;
    short	lbrk;
    int		number;


    if(linr < 2) return(-1);
    if(init==TRUE || *ppbrk == NULL) { /*must find breakpoint table*/
	pbrkTable = findBrkTable(linr);
	if(!pbrkTable) return(S_dbLib_badField);
	*ppbrk = (void *)pbrkTable;
	/* Just start at the beginning */
	*plbrk=0;
    }
    pbrkTable = (struct brkTable *)*ppbrk;
    number = pbrkTable->number;
    lbrk = *plbrk;
    /*make sure we dont go off end of table*/
    if( (lbrk+1) >= number ) lbrk--;
    pInt = pbrkTable->papBrkInt[lbrk];
    pnxtInt = pbrkTable->papBrkInt[lbrk+1];
    /* find entry for increased value */
    while( (pnxtInt->raw) <= val ) {
         lbrk++;
         pInt = pbrkTable->papBrkInt[lbrk];
         if( lbrk >= number-1) {
                status=1;
                break;
         }
         pnxtInt = pbrkTable->papBrkInt[lbrk+1];
    }
    while( (pInt->raw) > val) {
         if(lbrk==0) {
                status=1;
                break;
            }
         lbrk--;
         pInt = pbrkTable->papBrkInt[lbrk];
    }
    *plbrk = lbrk;
    *pval = pInt->eng + (val - pInt->raw) * pInt->slope;
    return(status);
}

long epicsShareAPI cvtEngToRawBpt(double *pval,short linr,short init,
	 void **ppbrk,short *plbrk)
{ 
     double	val=*pval;
     long	status=0;
     brkTable	*pbrkTable;
     brkInt	*pInt;     
     brkInt	*pnxtInt;
     short	lbrk;
     int	number;


     if(linr < 2) return(-1);
     if(init==TRUE || *ppbrk == NULL) { /*must find breakpoint table*/
	pbrkTable = findBrkTable(linr);
	if(!pbrkTable) return(S_dbLib_badField);
	*ppbrk = (void *)pbrkTable;
         /* Just start at the beginning */
         *plbrk=0;
     }
     pbrkTable = (struct brkTable *)*ppbrk;
     number = pbrkTable->number;
     lbrk = *plbrk;
     /*make sure we dont go off end of table*/
     if( (lbrk+1) >= number ) lbrk--;
     pInt = pbrkTable->papBrkInt[lbrk];
     pnxtInt = pbrkTable->papBrkInt[lbrk+1];
     /* find entry for increased value */
     while( (pnxtInt->eng) <= val ) {
         lbrk++;
         pInt = pbrkTable->papBrkInt[lbrk];
         if( lbrk >= number-1) {
                status=1;
                break;
         }
         pnxtInt = pbrkTable->papBrkInt[lbrk+1];
     }
     while( (pInt->eng) > val) {
         if(lbrk==0) {
                status=1;
                break;
            }
         lbrk--;
         pInt = pbrkTable->papBrkInt[lbrk];
     }
     if(pInt->slope!=0){
         *plbrk = lbrk;
         *pval = pInt->raw + (val - pInt->eng) / pInt->slope;
     }
     else {
         return(status);
     }
     return(0);
}
