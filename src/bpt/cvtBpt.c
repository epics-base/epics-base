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
#include     <vxWorks.h>
#include     <stdio.h>

#include     <ellLib.h>
#include     <dbBase.h>
#include     <dbStaticLib.h>
#include     <cvtTable.h>
#include     <epicsPrint.h>

extern struct dbBase *pdbbase;



static brkTable *findBrkTable(short linr)
{ 
    brkTable	*pbrkTable;
    dbMenu	*pdbMenu;
    char	name[50];
    char	*pname = name;
    int		len,ind;


    pdbMenu = dbFindMenu(pdbbase,"menuConvert");
    len = strlen(pdbMenu->papChoiceValue[linr]);
    if(len>=sizeof(name)) {
	epicsPrintf("Break Tables(findBrkTable) choice name too long\n");
	return(0);
    }
    strcpy(pname,pdbMenu->papChoiceValue[linr]);
    for(ind=0; ind<strlen(pname); ind++) {
	if(!isalnum(pname[ind])) {
	    pname[ind] = '\0';
	    break;
	}
    }
    pbrkTable = dbFindBrkTable(pdbbase,pname);
    return(pbrkTable);
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
