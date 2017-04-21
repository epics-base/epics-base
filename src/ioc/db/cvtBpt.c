/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
  
/* cvtBpt.c - Convert using breakpoint table
 *
 *      Author:          Marty Kraimer
 *      Date:            04OCT95
 *	This is adaptation of old bldCvtTable 
 */

#include "epicsPrint.h"

#define epicsExportSharedSymbols
#include "cvtTable.h"
#include "dbAccess.h"
#include "dbBase.h"
#include "dbStaticLib.h"

static brkTable *findBrkTable(short linr)
{ 
    dbMenu	*pdbMenu;

    pdbMenu = dbFindMenu(pdbbase,"menuConvert");
    if (!pdbMenu) {
	epicsPrintf("findBrkTable: menuConvert not loaded!\n");
	return NULL;
    }
    if (linr < 0 || linr >= pdbMenu->nChoice) {
	epicsPrintf("findBrkTable: linr=%d but menuConvert only has %d choices\n",
	    linr,pdbMenu->nChoice);
	return NULL;
    }
    return dbFindBrkTable(pdbbase,pdbMenu->papChoiceValue[linr]);
}

/* Used by both ao and ai record types */
long cvtRawToEngBpt(double *pval, short linr, short init,
	void **ppbrk, short *plbrk)
{
    double	val = *pval;
    long	status = 0;
    brkTable	*pbrkTable;
    brkInt	*pInt, *nInt;
    short	lbrk;
    int		number;

    if (linr < 2)
	return -1;

    if (init || *ppbrk == NULL) {
	pbrkTable = findBrkTable(linr);
	if (!pbrkTable)
	    return S_dbLib_badField;
	
	*ppbrk = (void *)pbrkTable;
	*plbrk = 0;
    } else
	pbrkTable = (brkTable *)*ppbrk;
    
    number = pbrkTable->number;
    lbrk = *plbrk;
    
    /* Limit index to the size of the table */
    if (lbrk < 0)
	lbrk = 0;
    else if (lbrk > number-2)
	lbrk = number-2;
    
    pInt = & pbrkTable->paBrkInt[lbrk];
    nInt = pInt + 1;
    
    if (nInt->raw > pInt->raw) {
	/* raw values increase down the table */
	while (val > nInt->raw) {
	    lbrk++;
	    pInt = nInt++;
	    if (lbrk > number-2) {
		status = 1;
		break;
	    }
	}
	while (val < pInt->raw) {
	    if (lbrk <= 0) {
		status = 1;
		break;
	    }
	    lbrk--;
	    nInt = pInt--;
	}
    } else {
	/* raw values decrease down the table */
	while (val <= nInt->raw) {
	    lbrk++;
	    pInt = nInt++;
	    if (lbrk > number-2) {
		status = 1;
		break;
	    }
	}
	while(val > pInt->raw) {
	    if (lbrk <= 0) {
		status = 1;
		break;
	    }
	    lbrk--;
	    nInt = pInt--;
	}
    }
    
    *plbrk = lbrk;
    *pval = pInt->eng + (val - pInt->raw) * pInt->slope;
    
    return status;
}

/* Used by the ao record type */
long cvtEngToRawBpt(double *pval, short linr, short init,
	void **ppbrk, short *plbrk)
{
    double	val = *pval;
    long	status = 0;
    brkTable	*pbrkTable;
    brkInt	*pInt, *nInt;
    short	lbrk;
    int		number;

    if (linr < 2)
	return -1;
    
    if (init || *ppbrk == NULL) { /*must find breakpoint table*/
	pbrkTable = findBrkTable(linr);
	if (!pbrkTable)
	    return S_dbLib_badField;
	
	*ppbrk = (void *)pbrkTable;
	/* start at the beginning */
	*plbrk = 0;
    } else
	pbrkTable = (brkTable *)*ppbrk;
    
    number = pbrkTable->number;
    lbrk = *plbrk;
    
    /* Limit index to the size of the table */
    if (lbrk < 0)
	lbrk = 0;
    else if (lbrk > number-2)
	lbrk = number-2;
    
    pInt = & pbrkTable->paBrkInt[lbrk];
    nInt = pInt + 1;
    
    if (nInt->eng > pInt->eng) {
	/* eng values increase down the table */
	while (val > nInt->eng) {
	    lbrk++;
	    pInt = nInt++;
	    if (lbrk > number-2) {
		status = 1;
		break;
	    }
	}
	while (val < pInt->eng) {
	    if (lbrk <= 0) {
		status = 1;
		break;
	    }
	    lbrk--;
	    nInt = pInt--;
	}
    } else {
	/* eng values decrease down the table */
	while (val <= nInt->eng) {
	    lbrk++;
	    pInt = nInt++;
	    if (lbrk > number-2) {
		status = 1;
		break;
	    }
	}
	while (val > pInt->eng) {
	    if (lbrk <= 0) {
		status = 1;
		break;
	    }
	    lbrk--;
	    nInt = pInt--;
	}
    }
    
    *plbrk = lbrk;
    *pval = pInt->raw + (val - pInt->eng) / pInt->slope;
    
    return status;
}
