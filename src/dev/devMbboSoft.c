/* devMbboSoft.c */
/* share/src/dev $Id$ */

/* devMbboSoft.c - Device Support Routines for  Soft Multibit Binary Output*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
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
 * .02  mm-dd-yy        iii     Comment
 *      ...
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
#include	<mbboRecord.h>


/* Create the dset for devMbboSoft */
long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboSoft={
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	write_mbbo};




static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    char message[100];
    long status;

    /* mbbo.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        status = dbPutLink(&pmbbo->out.value.db_link,pmbbo,DBR_USHORT,
                &pmbbo->val,1L);
        if(status!=0) {
                if(pmbbo->nsev<VALID_ALARM) {
                        pmbbo->nsev = VALID_ALARM;
                        pmbbo->nsta = LINK_ALARM;
                }
        }
        break;
    case (CA_LINK) :
        break;
    default :
        if(pmbbo->nsev<VALID_ALARM) {
                pmbbo->nsev = VALID_ALARM;
                pmbbo->nsta = SOFT_ALARM;
                if(pmbbo->stat!=SOFT_ALARM) {
                        strcpy(message,pmbbo->name);
                        strcat(message,": devMbboSoft (write_mbbo) Illegal OUT field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
