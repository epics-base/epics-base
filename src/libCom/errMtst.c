/* errMtst.c */
/* share/src/libCom @(#)errSymFind.c	1.3     2/4/92 */
/*
 *      Author:          Bob Zieman
 *      Date:            6-1-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  mm-dd-yy        iii     Comment
 */
/* errSymFind.c - Locate error symbol */


#include <error.h>
#ifdef vxWorks
#include <vxWorks.h>
#include <types.h>
#include <symLib.h>
extern caddr_t  statSymTbl;
#else
#include <string.h>
extern int      sys_nerr;
extern char    *sys_errlist[];
#endif


int 
errSymFind(status, name)
    long            status;
    char           *name;
{
    long            value;
#ifdef vxWorks
    unsigned char   type;
#endif
    unsigned short  modnum;
    modnum = (status >> 16);
    if (modnum <= 500)
#ifdef vxWorks
	symFindByValue((SYMTAB_ID)statSymTbl, status, name,(int*) &value, (SYM_TYPE*)&type);
#else
	UnixSymFind(status, name, &value);
#endif
    else
	ModSymFind(status, name, &value);
    if (value != status)
	return (-1);
    else
	return (0);
}

#ifndef vxWorks
int 
UnixSymFind(status, pname, pvalue)
    long            status;
    char           *pname;
    long           *pvalue;
{
    if (status >= sys_nerr || status < 1) {
	*pvalue = -1;
	return;
    }
    strcpy(pname, sys_errlist[status]);
    *pvalue = status;
    return;
}
#endif

int 
ModSymFind(status, pname, pvalue)
    long            status;
    char           *pname;
    long           *pvalue;
{
    unsigned short  modNum;
    unsigned short  modDim;
    unsigned short  errOff;
    unsigned short  errDim;
    /* lsb status must be odd */
    if (!(status & 0x1)) {
	*pvalue = -1;
	return;
    }
    modNum = (status >> 16);
    if (modNum < 501) {
	*pvalue = -1;
	return;
    }
    modNum = modNum - 501;
    if (!dbErrDes->papErrSet[modNum]) {
	*pvalue = -1;
	return;
    }
    modDim = dbErrDes->number;
    if (modNum >= modDim) {
	*pvalue = -1;
	return;
    }
    errDim = dbErrDes->papErrSet[modNum]->number;
    errOff = (status & 0xffff) >> 1;
    if (errOff >= errDim) {
	*pvalue = -1;
	return;
    }
    if (!dbErrDes->papErrSet[modNum]->papName[errOff]) {
	*pvalue = -1;
	return;
    }
    strcpy(pname, dbErrDes->papErrSet[modNum]->papName[errOff]);
    *pvalue = status;
    return;
}
