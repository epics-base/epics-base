
/* db_access.c */
/* share/src/db $Id$ */
/* db_access.c - Interface between old database access and new */
#include        <vxWorks.h>
#include        <types.h>

#include        <fioLib.h>
#include        <strLib.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <dbCommon.h>
#include        <errMdef.h>

#ifndef NULL
#define NULL 0
#endif

#define oldDBF_STRING      0
#define oldDBF_INT         1
#define oldDBF_FLOAT       2
#define oldDBF_ENUM        3
#define oldDBF_NO_ACCESS   4

/* data request buffer types */
#define oldDBR_STRING		oldDBF_STRING	
#define	oldDBR_INT		oldDBF_INT		
#define	oldDBR_FLOAT		oldDBF_FLOAT	
#define	oldDBR_ENUM		oldDBF_ENUM
#define oldDBR_STS_STRING	4
#define	oldDBR_STS_INT		5
#define	oldDBR_STS_FLOAT	6
#define	oldDBR_STS_ENUM		7
#define	oldDBR_GR_INT		8
#define	oldDBR_GR_FLOAT		9
#define oldDBR_CTRL_INT		10
#define	oldDBR_CTRL_FLOAT	11
#define oldDBR_CTRL_ENUM	12

/* structures for old database access*/
/* structure for a  string status field */
struct dbr_sts_string{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	char	value[MAX_STRING_SIZE];		/* current value */
};

/* structure for an integer status field */
struct dbr_sts_int{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	short	value;			/* current value */
};

/* structure for a  float status field */
struct dbr_sts_float{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	float	value;			/* current value */
};

/* structure for a  enum status field */
struct dbr_sts_enum{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	short	value;			/* current value */
};

	


/* structure for a graphic integer field */
struct dbr_gr_int{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	char	units[8];		/* units of value */
	short	upper_disp_limit;	/* upper limit of graph */
	short	lower_disp_limit;	/* lower limit of graph */
	short	upper_alarm_limit;	
	short	upper_warning_limit;
	short	lower_warning_limit;
	short	lower_alarm_limit;
	short	value;			/* current value */
};

/* structure for a graphic floating point field */
struct dbr_gr_float{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	short	precision;		/* number of decimal places */
	char	units[8];		/* units of value */
	float	upper_disp_limit;	/* upper limit of graph */
	float	lower_disp_limit;	/* lower limit of graph */
	float	upper_alarm_limit;	
	float	upper_warning_limit;
	float	lower_warning_limit;
	float	lower_alarm_limit;
	float	value;			/* current value */
};

/* structure for a control integer */
struct dbr_ctrl_int{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	char	units[8];		/* units of value */
	short	upper_disp_limit;	/* upper limit of graph */
	short	lower_disp_limit;	/* lower limit of graph */
	short	upper_alarm_limit;	
	short	upper_warning_limit;
	short	lower_warning_limit;
	short	lower_alarm_limit;
	short	upper_ctrl_limit;	/* upper control limit */
	short	lower_ctrl_limit;	/* lower control limit */
	short	value;			/* current value */
};

/* structure for a control floating point field */
struct dbr_ctrl_float{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	short	precision;		/* number of decimal places */
	char	units[8];		/* units of value */
	float	upper_disp_limit;	/* upper limit of graph */
	float	lower_disp_limit;	/* lower limit of graph */
	float	upper_alarm_limit;	
	float	upper_warning_limit;
	float	lower_warning_limit;
	float	lower_alarm_limit;
 	float	upper_ctrl_limit;	/* upper control limit */
	float	lower_ctrl_limit;	/* lower control limit */
	float	value;			/* current value */
};

/* structure for a control enumeration field */
struct dbr_ctrl_enum{
	short	status;	 		/* status of value */
	short	severity;		/* severity of alarm */
	short	no_str;			/* number of strings */
	char	strs[16][26];		/* state strings */
	short	value;			/* current value */
};


/* From $cs/dblib/src/dbiocsubs.c
 * subroutines
 *
 *      db_process      process a database record
 *          args
 *              pdb_addr        pointer to a database address
 *              command_mask    mask of operations to perform
 *      new_alarm       send an alarm message (NYI!!!!!)
 *      fill            fill a buffer with a character
 *          args
 *              pbuffer         pointer to the buffer
 *              size            number of characters
 *              fillchar        character with which to fill
 */

/*
 * DB_PROCESS
 *
 * process database records
 */
db_process(pdb_addr)
register struct db_addr *pdb_addr;
{
        long    status;

        status=dbProcess(pdb_addr);
        if(!RTN_SUCCESS(status)) errMessage(status,"db_process failed");
        return;
}

new_alarm(){
        return;
}
/*  FILL  fill a buffer with the designated character */
 fill(pbuffer,size,fillchar)
 register char  *pbuffer;
 register short size;
 register char  fillchar;
 {
        register short  i;

        for (i=0; i<size; i++){
                *pbuffer = fillchar;
                pbuffer++;
        }
}

/*	IOCDBACCESS.C 
 * Global Subroutines
 *
 * db_name_to_addr      converts a database name in the format "pv<.field>"
 *                      to a database address
 *    args
 *      pname           pointer to the database name
 *      paddr           pointer to the database address structure
 *    returns
 *      0               successful
 *      -1              not successful
 *              pv portion failed - node, record type and record number are -1
 *              field portion failed - field type, offset, size and ext are -1
 *
 *
 * db_get_field 	get a field from the database and convert it to
 * 			the specified type.
 *
 *    args
 *	paddr		pointer to the database address structure
 *	buffer_type	the type of data to return
 *			see DBR_ defines in db_access.h
 *	pbuffer		return buffer
 *	no_elements	number of elements
 *    returns
 *	0		successful
 *	-1		failed
 *
 * db_put_field 	put a converted buffer into the database
 *    args
 *	paddr		pointer to the database address structure
 *	buffer_type	the type of data to be converted and stored
 *			see DBR_ defines in db_access.h
 *	pbuffer		return buffer
 *    returns
 *	0		successful
 *	-1		failed
 *
 */

/*
 * DB_NAME_TO_ADDR
 */
static short mapNewToOld[]={0,1,1,1,1,2,2,3,4};

db_name_to_addr(pname,paddr)
register char           *pname;
register struct dbAddr *paddr;
{
        long    status;
	short   ftype;

        status=dbNameToAddr(pname,paddr);
        if(RTN_SUCCESS(status)) {
	    ftype = paddr->dbr_field_type;
	    if(ftype<0 || ftype> (sizeof(mapNewToOld)/sizeof(short))) {
		printf("db_name_to_addr: Illegal dbr_field_type\n");
		exit(-1);
	    }
	    paddr->dbr_field_type = mapNewToOld[paddr->dbr_field_type];
            return(0);
	}
        else
            return(-1);
}

/* DB_GET_FIELD get a field and convert it to the desired type */

db_get_field(paddr,buffer_type,pbuffer,no_elements)
struct dbAddr	*paddr;
short		buffer_type;
char		*pbuffer;
unsigned short	no_elements;
{
    long status;
    long options;
    long nRequest;
    long precision;
    short severity;


    switch(buffer_type) {
    case(oldDBR_STRING):
	options=0;
	nRequest=no_elements;
	status = dbGetField(paddr,DBR_STRING,pbuffer,&options,&nRequest);
	break;
    case(oldDBR_INT):
	options=0;
	nRequest=no_elements;
	status = dbGetField(paddr,DBR_SHORT,pbuffer,&options,&nRequest);
	break;
    case(oldDBR_FLOAT):
	options=0;
	nRequest=no_elements;
	status = dbGetField(paddr,DBR_FLOAT,pbuffer,&options,&nRequest);
	break;
    case(oldDBR_ENUM):
	options=0;
	nRequest=no_elements;
	status = dbGetField(paddr,DBR_ENUM,pbuffer,&options,&nRequest);
	break;
    case(oldDBR_STS_STRING):
	{
		struct dbr_sts_string *pold = (struct dbr_sts_string *)pbuffer;
		struct {
			DBRstatus
		} new;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_STRING,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_STRING,&(pold->value[0]),&options,&nRequest);
	}
	break;
    case(oldDBR_STS_INT):
	{
		struct dbr_sts_int *pold = (struct dbr_sts_int *)pbuffer;
		struct {
			DBRstatus
		} new;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_SHORT,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,&(pold->value),&options,&nRequest);
	}
	break;
    case(oldDBR_STS_FLOAT):
	{
		struct dbr_sts_float *pold = (struct dbr_sts_float *)pbuffer;
		struct {
			DBRstatus
		} new;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_FLOAT,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,&(pold->value),&options,&nRequest);
	}
	break;
    case(oldDBR_STS_ENUM):
	{
		struct dbr_sts_enum *pold = (struct dbr_sts_enum *)pbuffer;
		struct {
			DBRstatus
		} new;

		options=DBR_STATUS;
		nRequest=0;
		status = dbGetField(paddr,DBR_ENUM,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_ENUM,&(pold->value),&options,&nRequest);
	}
	break;
    case(oldDBR_GR_INT):
	{
		struct dbr_gr_int *pold = (struct dbr_gr_int *)pbuffer;
		struct {
			DBRstatus
			DBRunits
			DBRgrLong
			DBRalLong
		} new;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_SHORT,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		strncpy(pold->units,new.units,8);
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,&(pold->value),&options,&nRequest);
	}
	break;
    case(oldDBR_GR_FLOAT):
	{
		struct dbr_gr_float *pold = (struct dbr_gr_float *)pbuffer;
		struct {
			DBRstatus
			DBRunits
			DBRprecision
			DBRgrDouble
			DBRalDouble
		} new;

		options=DBR_STATUS|DBR_UNITS|DBR_PRECISION|DBR_GR_DOUBLE|DBR_AL_DOUBLE;
		nRequest=0;
		status = dbGetField(paddr,DBR_FLOAT,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		pold->precision = new.precision;
		strncpy(pold->units,new.units,8);
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,&(pold->value),&options,&nRequest);
	}
	break;
    case(oldDBR_CTRL_INT):
	{
		struct dbr_ctrl_int *pold = (struct dbr_ctrl_int *)pbuffer;
		struct {
			DBRstatus
			DBRunits
			DBRgrLong
			DBRctrlLong
			DBRalLong
		} new;

		options=DBR_STATUS|DBR_UNITS|DBR_GR_LONG|DBR_CTRL_LONG|DBR_AL_LONG;
		nRequest=0;
		status = dbGetField(paddr,DBR_SHORT,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		strncpy(pold->units,new.units,8);
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		pold->upper_ctrl_limit = new.upper_ctrl_limit;
		pold->lower_ctrl_limit = new.lower_ctrl_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_SHORT,&(pold->value),&options,&nRequest);
	}
	break;
    case(oldDBR_CTRL_FLOAT):
	{
		struct dbr_ctrl_float *pold = (struct dbr_ctrl_float *)pbuffer;
		struct {
			DBRstatus
			DBRunits
			DBRprecision
			DBRgrDouble
			DBRctrlDouble
			DBRalDouble
		} new;

		options=DBR_STATUS|DBR_UNITS|DBR_PRECISION|DBR_GR_DOUBLE|DBR_CTRL_DOUBLE|DBR_AL_DOUBLE;
		nRequest=0;
		status = dbGetField(paddr,DBR_FLOAT,&new,&options,&nRequest);
		pold->status = new.status;
		pold->severity = new.severity;
		adjust_severity(pbuffer);
		pold->precision = new.precision;
		strncpy(pold->units,new.units,8);
		pold->upper_disp_limit = new.upper_disp_limit;
		pold->lower_disp_limit = new.lower_disp_limit;
		pold->upper_alarm_limit = new.upper_alarm_limit;
		pold->upper_warning_limit = new.upper_warning_limit;
		pold->lower_warning_limit = new.lower_warning_limit;
		pold->lower_alarm_limit = new.lower_alarm_limit;
		pold->upper_ctrl_limit = new.upper_ctrl_limit;
		pold->lower_ctrl_limit = new.lower_ctrl_limit;
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_FLOAT,&(pold->value),&options,&nRequest);
	}
	break;
    case(oldDBR_CTRL_ENUM):
	{
		struct dbr_ctrl_enum *pold = (struct dbr_ctrl_enum *)pbuffer;
		struct {
			DBRstatus
			DBRenumStrs
		} new;
		short no_str,i;

		bzero(pold,sizeof(struct dbr_ctrl_enum));
		/* first get status and severity */
		options=DBR_STATUS|DBR_ENUM_STRS;
		nRequest=0;
		status = dbGetField(paddr,DBR_ENUM,&new,&options,&nRequest);
		adjust_severity(pold);
		no_str = new.no_str;
		if(no_str>16) no_str=16;
		pold->no_str = no_str;
		for (i=0; i<no_str; i++) 
			strncpy(pold->strs[i],new.strs[i],sizeof(pold->strs[i]));
		/*now get values*/
		options=0;
		nRequest=no_elements;
		status = dbGetField(paddr,DBR_ENUM,&(pold->value),&options,&nRequest);
	}
	break;
    default:
	return(-1);
    }
    if(!RTN_SUCCESS(status)) return(-1);
    return(0);
}

/* the old software did not have a severity of INFO */
static adjust_severity(pbuffer)
struct dbr_sts_string *pbuffer;
{
if (pbuffer->severity > 0) pbuffer->severity--;
}

/* DB_PUT_FIELD put a field and convert it to the desired type */

db_put_field(paddr,src_type,psrc,no_elements)
struct dbAddr	*paddr;		/* where to put it */
short		src_type,no_elements;
char			*psrc;		/* where to get it from */
{
    long status;


    switch(src_type) {
    case(oldDBR_STRING):
	status = dbPutField(paddr,DBR_STRING,psrc,(long)no_elements);
	break;
    case(oldDBR_INT):
	status = dbPutField(paddr,DBR_SHORT,psrc,(long)no_elements);
	break;
    case(oldDBR_FLOAT):
	status = dbPutField(paddr,DBR_FLOAT,psrc,(long)no_elements);
	break;
    case(oldDBR_ENUM):
	status = dbPutField(paddr,DBR_ENUM,psrc,(long)no_elements);
	break;
    case(oldDBR_CTRL_INT):
	status = dbPutField(paddr,DBR_SHORT,
	    &(((struct dbr_ctrl_int *)psrc)->value),(long)no_elements);
	break;
    case(oldDBR_CTRL_FLOAT):
	status = dbPutField(paddr,DBR_FLOAT,
	    &(((struct dbr_ctrl_float *)psrc)->value),(long)no_elements);
	break;
    default:
	return(-1);
    }
    if(!RTN_SUCCESS(status)) return(-1);
    return(0);
}
