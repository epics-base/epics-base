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
* int recGblGetTypeIndex(prec_name,ptypeIndex)
*     char	*prec_name;
*     int		*ptypeIndex;
*
* int recGblReportDbCommon(fp,paddr))
*     FILE		*fp;
*     struct dbAddr	*paddr;
*
* int recGblReportLink(fp,pfield_name,plink)
*     FILE		*fp;
*     char		*pfield_name;
*     struct link		*plink;
*
* int recGblReportCvtChoice(fp,pfield_name,choice_value)
*     FILE		*fp;
*     char		*pfield_name;
*     unsigned short	choice_value;
*
* int recGblReportGblChoice(fp,precord,pfield_name,choice_value)
*     FILE	    	*fp;
*     struct dbCommon 	*precord;
*     char	    	*pfield_name;
*     unsigned short	choice_value;
*
* int recGblReportRecChoice(fp,precord,pfield_name,choice_value)
*     FILE	    	*fp;
*     struct dbCommon 	*precord;
*     char	    	*pfield_name;
*     unsigned short	choice_value;
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
 
int recGblGetTypeIndex(prec_name,ptypeIndex)
    char	*prec_name;
    int		*ptypeIndex;
{
    int i;

    for(i=0; i < dbRecType->number; i++) {
	if(!(dbRecType->papName[i])) continue;
	if(strcmp(dbRecType->papName[i],prec_name) == 0) {
	    *ptypeIndex = i;
	    return(0);
	}
    }
    return(-1);
}

int recGblReportDbCommon(fp,paddr)
    FILE		*fp;
    struct dbAddr	*paddr;
{
    struct dbCommon	*precord=(struct dbCommon *)(paddr->precord);
    struct devSup *pdevSup;

    if(fprintf(fp,"NAME %-28s\nDESC %-28s\n",precord->name,precord->desc)<0)
	return(-1);
    if(recGblReportGblChoice(fp,precord,"SCAN",precord->scan)) return(-1);
    if(fprintf(fp,"PHAS %d\tEVNT %d\n",
		precord->phas,precord->evnt)<0) return(-1);
    if(fprintf(fp,"STAT %d\tSEVR %d\nDTYP %5d\n",
		precord->stat,precord->sevr,precord->dtyp)<0) return(-1);
    if(precord->dset != NULL) {
	if(!(pdevSup=GET_DEVSUP(paddr->record_type))) return(-1);
	if(fprintf(fp,"DSET %s\n",(pdevSup->dsetName[precord->dtyp]))<0)
	    return(-1);
    }
    if(recGblReportLink(fp,"SDIS",&(precord->sdis))) return(-1);
    if(fprintf(fp,"DISA %d\tPACT %d\t",precord->disa,precord->pact)<0)
	return(-1);
    if(fprintf(fp,"LSET %d\n",precord->lset)<0) return(-1);
    if(fprintf(fp,"ESEC 0x%lx\tNSEC 0x%lx\n",precord->esec,precord->nsec)<0)
	return(-1);
    return(0);
}

int recGblReportLink(fp,pfield_name,plink)
    FILE		*fp;
    char		*pfield_name;
    struct link		*plink;
{
    switch(plink->type) {
    case CONSTANT:
	if(fprintf(fp,"%4s %12.4G\n",
	    pfield_name,
	    plink->value.value)<0) return(-1);
	break;
    case PV_LINK:
	if(fprintf(fp,"%4s %28s.%4s\n",
	    pfield_name,
	    plink->value.pv_link.pvname,
	    plink->value.pv_link.fldname)<0) return(-1);
	break;
    case VME_IO:
	if(fprintf(fp,"%4s VME: card=%2d signal=%2d\n",
	    pfield_name,
	    plink->value.vmeio.card,plink->value.vmeio.card)<0) return(-1);
	break;
    case CAMAC_IO:
	if(fprintf(fp,
	    "%4s CAMAC: branch=%2d crate=%2d slot=%2d channel=%2d\n",
	    pfield_name,
	    plink->value.camacio.branch,plink->value.camacio.crate,
	    plink->value.camacio.slot,plink->value.camacio.channel)<0)
	    return(-1);
	break;
    case AB_IO:
	if(fprintf(fp,
	    "%4s ABIO: link=%2d adaptor=%2d card=%2d signal=%2d flag=%1d\n",
	    pfield_name,
	    plink->value.abio.link,plink->value.abio.adapter,
	    plink->value.abio.card,plink->value.abio.signal,
	    plink->value.abio.plc_flag)<0) return(-1);
	break;
    case GPIB_IO:
	if(fprintf(fp, "%4s GPIB: link=%2d taddr=%2d laddr=%2d signal=%3d\n",
	    pfield_name,
	    plink->value.gpibio.link,plink->value.gpibio.taddr,
	    plink->value.gpibio.laddr,plink->value.gpibio.signal)<0)
	    return(-1);
	break;
    case BITBUS_IO:
	if(fprintf(fp, "%4s BITBUS: link=%2d addr=%2d signal=%3d\n",
	    pfield_name,
	    plink->value.bitbusio.link,plink->value.bitbusio.addr,
	    plink->value.bitbusio.signal)<0) return(-1);
	break;
    case DB_LINK:
	if(fprintf(fp,"%4s DB_LINK: %28s\n",
	    pfield_name,
	    ((struct dbCommon *)(
		((struct dbAddr *)plink->value.db_link.paddr)
		->precord))->name)<0)
	    return(-1);
	break;
    case CA_LINK:
	if(fprintf(fp,"%4s CA_LINK: Not Yet Implemented\n",
	    pfield_name)<0) return(-1);
	break;
    default:
	errMessage(S_db_badField,"recGblReportLink: Illegal link.type");
	break;
    }
    return(0);
}

int recGblReportCvtChoice(fp,pfield_name,choice_value)
    FILE		*fp;
    char		*pfield_name;
    unsigned short	choice_value;
{
    char *pchoice;

    if(!(pchoice=GET_CHOICE(choiceCvt,choice_value))) {
	if(fprintf(fp,"%4s Illegal Choice\n",pfield_name)<0) return(-1);
    }
    else {
	if(fprintf(fp,"%4s: %s\n",pfield_name,pchoice)<0) return(-1);
    }
    return(0);
}


int recGblReportGblChoice(fp,precord,pfield_name,choice_value)
    FILE	    *fp;
    struct dbCommon *precord;
    char	    *pfield_name;
    unsigned short  choice_value;
{
    char	     name[PVNAME_SZ+1+FLDNAME_SZ+1];
    struct dbAddr    dbAddr;
    struct choiceSet *pchoiceSet;
    char	     *pchoice;

    strncpy(name,precord->name,PVNAME_SZ);
    strcat(name,".");
    strncat(name,pfield_name,FLDNAME_SZ);
    if(dbNameToAddr(name,&dbAddr)) {
	if(fprintf(fp,"%4s dbNameToAddr failed?\n",pfield_name)<0)return(-1);
	return(0);
    }
    if( !(pchoiceSet=GET_PCHOICE_SET(choiceGbl,dbAddr.choice_set))
    ||  !(pchoice=GET_CHOICE(pchoiceSet,choice_value))) {
	if(fprintf(fp,"%4s Cant find Choice\n",pfield_name)<0) return(-1);
    }
    else {
	if(fprintf(fp,"%4s: %s\n",pfield_name,pchoice)<0) return(-1);
    }
    return(0);
}

int recGblReportRecChoice(fp,precord,pfield_name,choice_value)
    FILE	    *fp;
    struct dbCommon *precord;
    char	    *pfield_name;
    unsigned short  choice_value;
{
    char	        name[PVNAME_SZ+1+FLDNAME_SZ+1];
    struct dbAddr       dbAddr;
    struct arrChoiceSet *parrChoiceSet;
    struct choiceSet    *pchoiceSet;
    char	        *pchoice;

    strncpy(name,precord->name,PVNAME_SZ);
    strcat(name,".");
    strncat(name,pfield_name,FLDNAME_SZ);
    if(dbNameToAddr(name,&dbAddr)) {
	if(fprintf(fp,"%4s dbNameToAddr failed?\n",pfield_name)<0)return(-1);
	return(0);
    }
    if( !(parrChoiceSet=GET_PARR_CHOICE_SET(choiceRec,dbAddr.record_type)) 
    ||  !(pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,dbAddr.choice_set))
    ||  !(pchoice=GET_CHOICE(pchoiceSet,choice_value))) {
	if(fprintf(fp,"%4s Cant find Choice\n",pfield_name)<0) return(-1);
    }
    else {
	if(fprintf(fp,"%4s: %s\n",pfield_name,pchoice)<0) return(-1);
    }
    return(0);
}
