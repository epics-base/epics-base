/* recGbl.c - Global record processing routines */
/* share/src/db $Id$ */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<strLib.h>

#include	<choice.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbRecType.h>
#include	<dbRecDes.h>
#include	<link.h>
#include	<devSup.h>
#include	<dbCommon.h>

/***********************************************************************
* The following are the global record processing rouitines
*
*void recGblDbaddrError(status,paddr,pcaller_name)
*     long              status;
*     struct dbAddr	*paddr;
*     char		*pcaller_name;	* calling routine name *
*
*void recGblRecordError(status,precord,pcaller_name)
*     long              status;
*     caddr_t		precord;	* addr of record	*
*     char		*pcaller_name;	* calling routine name *
*
*void recGblRecSupError(status,paddr,pcaller_name,psupport_name)
*     long              status;
*     struct dbAddr	*paddr;
*     char		*pcaller_name;	* calling routine name *
*     char		*psupport_name;	* support routine name	*
*
**************************************************************************/

recGblDbaddrError(status,paddr,pcaller_name)
long		status;
struct dbAddr	*paddr;	
char		*pcaller_name;	/* calling routine name*/
{
	char		buffer[200];
	struct dbCommon *precord;
	int		i,n;
	struct fldDes	*pfldDes=(struct fldDes *)(paddr->pfldDes);

	buffer[0]=0;
	if(paddr) { /* print process variable name */
		precord=(struct dbCommon *)(paddr->precord);
		strcat(buffer,"PV: ");
		strncat(buffer,precord->name,PVNAME_SZ);
		n=strlen(buffer);
		for(i=n; (i>0 && buffer[i]==' '); i--) buffer[i]=0;
		strcat(buffer,".");
		strncat(buffer,pfldDes->fldname,FLDNAME_SZ);
		strcat(buffer," ");
	}
	if(pcaller_name) {
		strcat(buffer,"error detected in routine: ");
		strcat(buffer,pcaller_name);
	}
	errMessage(status,buffer);
	return;
}

recGblRecordError(status,precord,pcaller_name)
long		status;
struct dbCommon *precord;	
char		*pcaller_name;	/* calling routine name*/
{
	char buffer[200];
	int i,n;

	buffer[0]=0;
	if(precord) { /* print process variable name */
		strcat(buffer,"PV: ");
		strncat(buffer,precord->name,PVNAME_SZ);
		n=strlen(buffer);
		for(i=n; (i>0 && buffer[i]==' '); i--) buffer[i]=0;
		strcat(buffer,"  ");
	}
	if(pcaller_name) {
		strcat(buffer,"error detected in routine: ");
		strcat(buffer,pcaller_name);
	}
	errMessage(status,buffer);
	return;
}

recGblRecSupError(status,paddr,pcaller_name,psupport_name)
long		status;
struct dbAddr	*paddr;
char		*pcaller_name;
char		*psupport_name;
{
	char buffer[200];
	char *pstr;
	struct dbCommon *precord;
	int		i,n;
	struct fldDes	*pfldDes=(struct fldDes *)(paddr->pfldDes);

	buffer[0]=0;
	strcat(buffer,"Record Support Routine (");
	if(psupport_name)
		strcat(buffer,psupport_name);
	else
		strcat(buffer,"Unknown");
	strcat(buffer,") not available.\nRecord Type is ");
	if(pstr=GET_PRECTYPE(paddr->record_type))
		strcat(buffer,pstr);
	else
		strcat(buffer,"BAD");
	if(paddr) { /* print process variable name */
		precord=(struct dbCommon *)(paddr->precord);
		strcat(buffer,", PV is ");
		strncat(buffer,precord->name,PVNAME_SZ);
		n=strlen(buffer);
		for(i=n; (i>0 && buffer[i]==' '); i--) buffer[i]=0;
		strcat(buffer,".");
		strncat(buffer,pfldDes->fldname,FLDNAME_SZ);
		strcat(buffer,"  ");
	}
	if(pcaller_name) {
		strcat(buffer,"\nerror detected in routine: ");
		strcat(buffer,pcaller_name);
	}
	errMessage(status,buffer);
	return;
}
