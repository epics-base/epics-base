/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbCaTest.c */

/****************************************************************
*
*	Current Author:		Marty Kraimer
*	Date:			10APR96
*
****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dbStaticLib.h"
#include "epicsStdioRedirect.h"
#include "link.h"
/*definitions needed because of old vs new database access*/
#undef DBR_SHORT
#undef DBR_PUT_ACKT
#undef DBR_PUT_ACKS
#undef VALID_DB_REQ
#undef INVALID_DB_REQ
/*end of conflicting definitions*/
#include "cadef.h"
#include "db_access.h"
#include "dbDefs.h"
#include "epicsPrint.h"
#include "dbCommon.h"
#include "epicsEvent.h"

/*define DB_CONVERT_GBLSOURCE because db_access.c does not include db_access.h*/
#define DB_CONVERT_GBLSOURCE

#define epicsExportSharedSymbols
#include "dbLock.h"
#include "db_access_routines.h"
#include "db_convert.h"
#include "dbCa.h"
#include "dbCaPvt.h"
#include "dbCaTest.h"


long epicsShareAPI dbcar(char	*precordname,int level)
{
    DBENTRY		dbentry;
    DBENTRY		*pdbentry=&dbentry;
    long		status;
    dbCommon		*precord;
    dbRecordType		*pdbRecordType;
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

    if (precordname && ((*precordname == '\0') || !strcmp(precordname,"*")))
        precordname = NULL;
    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecordType(pdbentry);
    while(!status) {
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    if(!precordname
	    || (strcmp(precordname,dbGetRecordName(pdbentry)) ==0)) {
		pdbRecordType = pdbentry->precordType;
		precord = (dbCommon *)pdbentry->precnode->precord;
		for(j=0; j<pdbRecordType->no_links; j++) {
		    pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
		    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
                    dbLockSetGblLock();
		    if (plink->type == CA_LINK) {
			ncalinks++;
			pca = (caLink *)plink->value.pv_link.pvt;
			if(pca
                        && pca->chid
                        && (ca_field_type(pca->chid) != TYPENOTCONN)) {
			    nconnected++;
			    nDisconnect += pca->nDisconnect;
			    nNoWrite += pca->nNoWrite;
			    if(!ca_read_access(pca->chid)) noReadAccess++;
			    if(!ca_write_access(pca->chid)) noWriteAccess++;
			    if(level>1) {
				printf("    connected ");
				printf("%s",ca_host_name(pca->chid));
				if(!ca_read_access(pca->chid))
					printf(" no_read_access");
				if(!ca_write_access(pca->chid))
					printf(" no_write_access");
				printf(" %s.%s %s",
				    precord->name,
				    pdbFldDes->name,
				    plink->value.pv_link.pvname);
				if(nDisconnect)
				    printf(" nDisconnect %lu",pca->nDisconnect);
				if(nNoWrite)
				    printf(" nNoWrite %lu",pca->nNoWrite);
				printf("\n");
			    }
			} else { 
			    if(level>0) {
				printf("not_connected %s.%s %s",
				    precord->name,
				    pdbFldDes->name,
				    plink->value.pv_link.pvname);
				if(nDisconnect)
				    printf(" nDisconnect %lu",pca->nDisconnect);
				if(nNoWrite)
				    printf(" nNoWrite %lu",pca->nNoWrite);
				printf("\n");
			    }
			}
		    }
                    dbLockSetGblUnlock();
		}
		if(precordname) goto done;
	    }
	    status = dbNextRecord(pdbentry);
	}
	status = dbNextRecordType(pdbentry);
    }
done:
    printf("ncalinks %d",ncalinks);
    printf(" not connected %d",(ncalinks - nconnected));
    printf(" no_read_access %d",noReadAccess);
    printf(" no_write_access %d\n",noWriteAccess);
    printf(" nDisconnect %lu nNoWrite %lu\n",nDisconnect,nNoWrite);
    dbFinishEntry(pdbentry);
    
    if ( level > 2  && dbCaClientContext != 0 ) {
        ca_context_status ( dbCaClientContext, level - 2 );
    }

    return(0);
}
