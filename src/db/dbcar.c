/* dbcar.c */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.
 
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

/****************************************************************
*
*	Current Author:		Marty Kraimer
*	Date:			10APR96
*
*
* Modification Log:
* -----------------
* .01  10APR96	mrk	list db to CA links
****************************************************************/

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <taskLib.h>

#include "dbStaticLib.h"
#include "link.h"
/*definitions needed because of old vs new database access*/
#undef DBR_SHORT
#undef DBR_PUT_ACKT
#undef DBR_PUT_ACKS
#undef VALID_DB_REQ
#undef INVALID_DB_REQ
/*end of conflicting definitions*/
#include "cadef.h"
#include "epicsPrint.h"
#include "dbCommon.h"
#include "dbCa.h"
extern struct dbBase *pdbbase;

long dbcar(char	*precordname,int level)
{
    DBENTRY		dbentry;
    DBENTRY		*pdbentry=&dbentry;
    long		status;
    dbCommon		*precord;
    dbRecDes		*pdbRecDes;
    dbFldDes		*pdbFldDes;
    DBLINK		*plink;
    int			ncalinks=0;
    int			nconnected=0;
    int			noReadAccess=0;
    int			noWriteAccess=0;
    unsigned long	nDisconnect=0;
    unsigned long	nNoWrite=0;
    caLink		*pca;
    int			j;


    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecdes(pdbentry);
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    if(!precordname
	    || (strcmp(precordname,dbGetRecordName(pdbentry)) ==0)) {
		pdbRecDes = pdbentry->precdes;
		precord = (dbCommon *)pdbentry->precnode->precord;
		for(j=0; j<pdbRecDes->no_links; j++) {
		    pdbFldDes = pdbRecDes->papFldDes[pdbRecDes->link_ind[j]];
		    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		    if (plink->type == CA_LINK) {
			ncalinks++;
			pca = (caLink *)plink->value.pv_link.pvt;
			if(pca && (ca_field_type(pca->chid) != TYPENOTCONN)) {
			    nconnected++;
			    nDisconnect += pca->nDisconnect;
			    nNoWrite += pca->nNoWrite;
			    if(!ca_read_access(pca->chid)) noReadAccess++;
			    if(!ca_write_access(pca->chid)) noWriteAccess++;
			    if(level>1) {
				printf("    connected");
				printf("%s",ca_host_name(pca->chid));
				if(!ca_read_access(pca->chid))
					printf(" no_read_access");
				if(!ca_write_access(pca->chid))
					printf(" no_write_access");
				printf(" %s.%s %s",
				    precord->name,
				    pdbFldDes->name,
				    plink->value.pv_link.pvname);
				printf(" nDisconnect %lu nNoWrite %lu\n",
				    pca->nDisconnect,pca->nNoWrite);
			    }
			} else { 
			    if(level>0) {
				printf("not_connected %s.%s",
					precord->name,pdbFldDes->name);
				printf(" nDisconnect %lu nNoWrite %lu\n",
				    pca->nDisconnect,pca->nNoWrite);
			    }
			}
		    }
		}
		if(precordname) goto done;
	    }
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecdes(pdbentry);
    }
done:
    printf("ncalinks %d",ncalinks);
    printf(" not connected %d",(ncalinks - nconnected));
    printf(" no_read_access %d",noReadAccess);
    printf(" no_write_access %d\n",noWriteAccess);
    printf(" nDisconnect %lu nNoWrite %lu\n",nDisconnect,nNoWrite);
    dbFinishEntry(pdbentry);
    return(0);
}
