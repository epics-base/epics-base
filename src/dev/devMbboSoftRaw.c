/* devMbboSoftRaw.c */
/* share/src/dev $Id$ */

/* devMbboSoftRaw.c - Device Support Routines for  SoftRaw Multibit Binary Output*/
/*
 *      Author:		Janet Anderson
 *      Date:		3-28-92
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
 * .01  03-28-92        jba     Initial version
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<mbboRecord.h>


/* Create the dset for devMbboSoftRaw */
long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboSoftRaw={
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
        status = dbPutLink(&pmbbo->out.value.db_link,(struct dbCommon *)pmbbo,DBR_USHORT,
                &pmbbo->rval,1L);
        if(status!=0) {
                recGblSetSevr(pmbbo,LINK_ALARM,VALID_ALARM);
        }
        break;
    case (CA_LINK) :
        break;
    default :
        if(recGblSetSevr(pmbbo,SOFT_ALARM,VALID_ALARM)){
                if(pmbbo->stat!=SOFT_ALARM) {
                        strcpy(message,pmbbo->name);
                        strcat(message,": devMbboSoftRaw (write_mbbo) Illegal OUT field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
