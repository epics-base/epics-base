/* setMasterTimeToSelf.c	ioc initialization */ 
/* share/src/db $Id$ */
/*
 *      Author:		Bob Zieman
 *      Date:		09-11-92
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
 * .01  09-11-92	rcz	moved here from iocInit.c
 *
 */

/*
 * SETMASTERTIMETOSELF
 *
 * This routine should only be called from function initHooks
 *	at case INITHOOKafterSetEnvParams or later but before
 *	INITHOOKafterTS_init 
 *
 */




#include	<vxWorks.h>
#include	<sysLib.h>
#include	<symLib.h>
#include	<sysSymTbl.h>
#include        <a_out.h>       /* for N_TEXT */




void setMasterTimeToSelf()
{
    BOOT_PARAMS     bp;
    char           *pnext;
    char            name[] = "_EPICS_IOCMCLK_INET";
    char            message[100];
    UTINY           type;
    long            rtnval = 0;
    char           *pSymAddr;
    int             len = 0;
    int             i = 0;
    char            *ptr = 0;

    pnext = bootStringToStruct(sysBootLine, &bp);
    if (*pnext != EOS) {
	sprintf(message,
		"setMasterTimeToSelf: unable to parse boot params\n");
	errMessage(-1L, message);
	return;
    }
    rtnval = symFindByName(sysSymTbl, name, &pSymAddr, &type);
    if (rtnval != OK || (type & N_TEXT == 0)) {
	sprintf(message,
		"setMasterTimeToSelf: symBol EPICS_IOCMCLK_INET not found");
	errMessage(-1L, message);
	return;
    }
    ptr = (char*)&bp.ead;
    len=strlen((char*)&bp.ead);
    /* strip off mask */
    for (i=0; i<len; i++, ptr++) {
	if ( *ptr == ':' ){
		break;
	}
    }
    *ptr = 0;
    ptr = (char*)&bp.ead;
    len=strlen(ptr);
    strncpy((char*)(pSymAddr + sizeof(void *)), ptr,len+1);
    return;
}
