/* devStringoutSoft.c */
 /* share/src/dev   $Id$ */

/* devStringoutSoft.c - Device Support Routines for Soft String Output */
/*
 *      Author:		Janet Anderson
 *      Date:   	04-21-91
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

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<stringoutRecord.h>

/* Create the dset for devStringoutSoft */
long write_stringout();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;
}devStringoutSoft={
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	write_stringout};
 

static long write_stringout(pstringout)
    struct stringoutRecord	*pstringout;
{
    char message[100];
    long status;

    /* stringout.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pstringout->out.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	status = dbPutLink(&pstringout->out.value.db_link,pstringout,DBR_STRING,
		pstringout->val,1L);
        if(status!=0) {
                if(pstringout->nsev<VALID_ALARM) {
                        pstringout->nsev = VALID_ALARM;
                        pstringout->nsta = LINK_ALARM;
                }
        }
	break;
    case (CA_LINK) :
	break;
    default :
	if(pstringout->nsev<VALID_ALARM) {
		pstringout->nsev = VALID_ALARM;
		pstringout->nsta = SOFT_ALARM;
		if(pstringout->stat!=SOFT_ALARM) {
			strcpy(message,pstringout->name);
			strcat(message,": devStringoutSoft (write_stringout) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
