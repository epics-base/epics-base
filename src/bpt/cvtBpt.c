/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */
  
/* cvtBpt.c - Convert using breakpoint table
 *
 *      Author:          Marty Kraimer
 *      Date:            04OCT95
 *	This is adaptation of old bldCvtTable 
 *
 */
#include     <vxWorks.h>
#include     <stdlib.h>
#include     <stdio.h>
#include     <string.h>
#include     <ctype.h>

#include     <ellLib.h>
#include     <dbBase.h>
#include     <dbStaticLib.h>
#include     <cvtTable.h>
#include     <epicsPrint.h>

extern struct dbBase *pdbbase;



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

long cvtRawToEngBpt(double *pval,short linr,short init, void **ppbrk,
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

long cvtEngToRawBpt(double *pval,short linr,short init,
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
