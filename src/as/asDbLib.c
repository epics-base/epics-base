/* share/src/as/dbAsLib.c	*/
/* share/src/as $Id$ */
/*
 *      Author:  Marty Kraimer
 *      Date:    02-11-94
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
 * .01  02-11-94	mrk	Initial Implementation
 */
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <dbDefs.h>
#include <dbStaticLib.h>
#include <asLib.h>
#include <asDbLib.h>
#include <dbCommon.h>

extern struct dbBase *pdbBase;
static FILE *stream;

#define BUF_SIZE 100
static char *my_buffer;
static char *my_buffer_ptr=NULL;

static int my_yyinput(char *buf, int max_size)
{
    int	l,n;
    
    if(!my_buffer_ptr || *my_buffer_ptr==0) {
	if(fgets(my_buffer,BUF_SIZE,stream)==NULL) return(0);
	my_buffer_ptr = my_buffer;
    }
    l = strlen(my_buffer_ptr);
    n = (l<=max_size ? l : max_size);
    memcpy(buf,my_buffer_ptr,n);
    my_buffer_ptr += n;
    return(n);
}

static long asDbAddRecords(void)
{
    DBENTRY		dbentry;
    DBENTRY		*pdbentry=&dbentry;
    long		status;
    struct dbCommon	*precord;

    dbInitEntry(pdbBase,pdbentry);
    status = dbFirstRecdes(pdbentry);
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    precord = pdbentry->precnode->precord;
	    if(!precord->asp) {
		status = asAddMember((ASMEMBERPVT *)&precord->asp,precord->asg);
		if(status) errMessage(status,"asDbAddRecords:asAddMember");
		asPutMemberPvt(precord->asp,precord);
	    }
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecdes(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return(0);
}

int asInit(char *filename)
{
    long status;

    my_buffer = malloc(BUF_SIZE);
    if(!my_buffer) {
	errMessage(0,"asInit malloc failure");
	return(-1);
    }
    stream = fopen(filename,"r");
    if(!stream) {
	errMessage(0,"asInit failure");
	return(-1);
    }
    status = asInitialize(my_yyinput);
    if(fclose(stream)==EOF) errMessage(0,"asInit fclose failure");
    free((void *)my_buffer);
    asDbAddRecords();
    return(0);
}

void static myMemberCallback(ASMEMBERPVT memPvt)
{
    struct dbCommon	*precord;

    precord = asGetMemberPvt(memPvt);
    if(precord) printf(" Record:%s",precord->name);
}

int asdbdump(void)
{
    asDump(myMemberCallback,NULL);
    return(0);
}

int asDbGetAsl(void *paddress)
{
    struct dbAddr	*paddr = paddress;
    struct fldDes	*pflddes;

    pflddes = paddr->pfldDes;
    return((int)pflddes->as_level);
}

ASMEMBERPVT  asDbGetMemberPvt(void *paddress)
{
    struct dbAddr	*paddr = paddress;
    struct dbCommon	*precord;

    precord = paddr->precord;
    return((ASMEMBERPVT)precord->asp);
}

int astac(char *pname,char *user,char *location)
{
    struct dbAddr	*paddr;
    long		status;
    ASCLIENTPVT		*pasclientpvt=NULL;
    struct dbCommon	*precord;
    struct fldDes	*pflddes;

    paddr = dbCalloc(1,sizeof(struct dbAddr) + sizeof(ASCLIENTPVT));
    pasclientpvt = (ASCLIENTPVT *)(paddr + 1);
    status=dbNameToAddr(pname,paddr);
    if(status) {
	errMessage(status,"dbNameToAddr error");
	return(1);
    }
    precord = paddr->precord;
    pflddes = paddr->pfldDes;
    status = asAddClient(pasclientpvt,(ASMEMBERPVT)precord->asp,
	(int)pflddes->as_level,user,location);
    if(status) {
	errMessage(status,"asAddClient error");
	return(1);
    }
    return(0);
}
