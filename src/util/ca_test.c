/*	$Id$
 *	Author:	Jeff Hill
 *	Date:	07-01-91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	07-01-91	joh	initial version
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 */

#include        <cadef.h>
#include        <vxWorks.h>

void 		printit();
void 		verify();

static char	outstanding;

#ifndef vxWorks
main(argc,argv)
int     argc;
char    **argv;
{

	if(argc < 2  || argc > 3){
		printf("usage: cagft <channel name>\n");
		return ERROR;
	}


	if(argc == 2){
		return ca_test(argv[1], NULL);
	}
	else if(argc == 3){
		return ca_test(argv[1], argv[2]);
	}
	else{
		return ERROR;
	}

}
#endif


int ca_test(pname, pvalue )
char	*pname;
char	*pvalue;
{

	if(pvalue){
		return capft(pname,pvalue);
	}
	else{
		return cagft(pname);
	}
}



int cagft(pname)
char	*pname;
{	
	chid		chan_id;
	int		status;
	int		i;
	unsigned	ntries = 10;

	/* 
	 *	convert name to chan id 
	 */
	status = ca_search(pname, &chan_id);
	SEVCHK(status,NULL);
	status = ca_pend_io(2.0);
	if(status != ECA_NORMAL){
		printf("Not Found %s\n", pname);
		return ERROR;
	}

	printf("name:\t%s\n", ca_name(chan_id));
	printf("native type:\t%d\n", ca_field_type(chan_id));
	printf("native count:\t%d\n", ca_element_count(chan_id));


	/* 
 	 * fetch as each type 
	 */
	for(i=0; i<=LAST_BUFFER_TYPE; i++){
		status = ca_array_get_callback(
				i, 
				ca_element_count(chan_id),
				chan_id, 
				printit, 
				NULL);
		SEVCHK(status, NULL);

		outstanding++;
	}

	while(ntries){
		ca_pend_event(1.0);

		if(!outstanding){
			printf("\n\n");
			return OK;
		}

		ntries--;
	}

	return	ERROR;
}


/*
 *	PRINTIT()
 */
static void printit(args)
struct  event_handler_args	args;
{
	int	i;

	print_returned(
		args.type,
		args.dbr,
		args.count);

	outstanding--;
}




/*
 */
int capft(pname,pvalue)
char		*pname;
char		*pvalue;
{
	short			shortvalue;
	long			longvalue;
	float			floatvalue;
	unsigned char		charvalue;
	double			doublevalue;
	unsigned		ntries = 10;
	int			status;
	chid			chan_id;

	if (((*pname < ' ') || (*pname > 'z'))
	  || ((*pvalue < ' ') || (*pvalue > 'z'))){
		printf("\nusage \"pv name\",\"value\"\n");
		return ERROR;
	}

	/* 
	 *	convert name to chan id 
	 */
	status = ca_search(pname, &chan_id);
	SEVCHK(status,NULL);
	status = ca_pend_io(2.0);
	if(status != ECA_NORMAL){
		printf("Not Found %s\n", pname);
		return ERROR;
	}

	printf("name:\t%s\n", ca_name(chan_id));
	printf("native type:\t%d\n", ca_field_type(chan_id));
	printf("native count:\t%d\n", ca_element_count(chan_id));

	status = ca_put(
			DBR_STRING, 
			chan_id, 
			pvalue);
	SEVCHK(status, NULL);
	verify(chan_id, DBR_STRING);

	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
		status = ca_put(
				DBR_SHORT, 
				chan_id, 
				&shortvalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_SHORT);
		status = ca_put(
				DBR_ENUM, 
				chan_id, 
				&shortvalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_ENUM);
		charvalue=shortvalue;
		status = ca_put(
				DBR_CHAR, 
				chan_id, 
				&charvalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_CHAR);
	}
	if(sscanf(pvalue,"%ld",&longvalue)==1) {
		status = ca_put(
				DBR_LONG, 
				chan_id, 
				&longvalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_LONG);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
		status = ca_put(
				DBR_FLOAT, 
				chan_id, 
				&floatvalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_FLOAT);
	}
	if(sscanf(pvalue,"%lf",&doublevalue)==1) {
		status = ca_put(
				DBR_DOUBLE, 
				chan_id, 
				&doublevalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_DOUBLE);
	}

	while(ntries){
		ca_pend_event(1.0);

		if(!outstanding){
			printf("\n\n");
			return OK;
		}

		ntries--;
	}

	return	ERROR;
}


/*
 * VERIFY
 *
 * print out the values in a database access interface structure
 */
static void verify(chan_id, type)
chid		chan_id;
int		type;
{
	int status;

	status = ca_array_get_callback(
			type, 
			ca_element_count(chan_id),
			chan_id, 
			printit, 
			NULL);
	SEVCHK(status, NULL);

	outstanding++;
}


/*
 * PRINT_RETURNED
 *
 * print out the values in a database access interface structure
 */
static print_returned(type,pbuffer,count)
register short	type;
register char	*pbuffer;
short		count;
{
    register short i;

    switch(type){
	case (DBR_STRING):
		printf("\n\tDBR_STRING Value: %s",pbuffer);
		break;
	case (DBR_SHORT):
	{
		register short *pvalue = (short *)pbuffer;
		printf("\n\tDBR_SHORT ");
		for (i = 0; i < count; i++,pvalue++){
			printf("%d ",*(short *)pvalue);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_FLOAT):
	{
		register float *pvalue = (float *)pbuffer;
		printf("\n\tDBR_FLOAT ");
		for (i = 0; i < count; i++,pvalue++){
			printf("%6.4f ",*(float *)pvalue);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_ENUM):
	{
		register short *pvalue = (short *)pbuffer;
		printf("\n\tDBR_ENUM  %d",*pvalue);
		break;
	}
	case (DBR_CHAR):
	{
		int	value;

		printf("\n\tDBR_CHAR ");
		for (i = 0; i < count; i++,pbuffer++){
			value = *(unsigned char *)pbuffer;
			printf("%d ",value);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_LONG):
	{
		register long *pvalue = (long *)pbuffer;
		printf("\n\tDBR_LONG ");
		for (i = 0; i < count; i++,pvalue++){
			printf("0x%x ",*pvalue);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_DOUBLE):
	{
		register double *pvalue = (double *)pbuffer;
		printf("\n\tDBR_DOUBLE ");
		for (i = 0; i < count; i++,pvalue++){
			printf("%6.4f ",(float)(*pvalue));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_STS_STRING):
	case (DBR_GR_STRING):
	case (DBR_CTRL_STRING):
	{
		register struct dbr_sts_string *pvalue 
		  = (struct dbr_sts_string *) pbuffer;
		printf("\n\tDBR_STS_STRING %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %s",pvalue->value);
		break;
	}
	case (DBR_STS_SHORT):
	{
		register struct dbr_sts_short *pvalue
		  = (struct dbr_sts_short *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("\n\tDBR_STS_SHORT %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			printf("%d ",*pshort);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_STS_FLOAT):
	{
		register struct dbr_sts_float *pvalue
		  = (struct dbr_sts_float *)pbuffer;
		register float *pfloat = &pvalue->value;
		printf("\n\tDBR_STS_FLOAT %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			printf("%6.4f ",*pfloat);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_STS_ENUM):
	{
		register struct dbr_sts_enum *pvalue
		  = (struct dbr_sts_enum *)pbuffer;
		printf("\n\tDBR_STS_ENUM %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %d",pvalue->value);
		break;
	}
	case (DBR_STS_CHAR):
	{
		register struct dbr_sts_char *pvalue
		  = (struct dbr_sts_char *)pbuffer;
		register unsigned char *pchar = &pvalue->value;
		short value;

		printf("\n\tDBR_STS_CHAR %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			value=*pchar;
			printf("%d ",value);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_STS_LONG):
	{
		register struct dbr_sts_long *pvalue
		  = (struct dbr_sts_long *)pbuffer;
		register long *plong = &pvalue->value;
		printf("\n\tDBR_STS_LONG %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			printf("0x%lx ",*plong);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_STS_DOUBLE):
	{
		register struct dbr_sts_double *pvalue
		  = (struct dbr_sts_double *)pbuffer;
		register double *pdouble = &pvalue->value;
		printf("\n\tDBR_STS_DOUBLE %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			printf("%6.4f ",(float)(*pdouble));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_TIME_STRING):
	{
		register struct dbr_time_string *pvalue 
		  = (struct dbr_time_string *) pbuffer;
		printf("\n\tDBR_TIME_STRING %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %lx %lx ",pvalue->stamp.secPastEpoch,pvalue->stamp.nsec);
		break;
	}
	case (DBR_TIME_SHORT):
	{
		register struct dbr_time_short *pvalue
		  = (struct dbr_time_short *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("\n\tDBR_TIME_SHORT %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %lx %lx ",pvalue->stamp.secPastEpoch,pvalue->stamp.nsec);
		for (i = 0; i < count; i++,pshort++){
			printf("%d ",*pshort);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_TIME_FLOAT):
	{
		register struct dbr_time_float *pvalue
		  = (struct dbr_time_float *)pbuffer;
		register float *pfloat = &pvalue->value;
		printf("\n\tDBR_TIME_FLOAT %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %lx %lx ",pvalue->stamp.secPastEpoch,pvalue->stamp.nsec);
		for (i = 0; i < count; i++,pfloat++){
			printf("%6.4f ",*pfloat);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_TIME_ENUM):
	{
		register struct dbr_time_enum *pvalue
		  = (struct dbr_time_enum *)pbuffer;
		printf("\n\tDBR_TIME_ENUM %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %lx %lx ",pvalue->stamp.secPastEpoch,pvalue->stamp.nsec);
		break;
	}
	case (DBR_TIME_CHAR):
	{
		register struct dbr_time_char *pvalue
		  = (struct dbr_time_char *)pbuffer;
		register unsigned char *pchar = &pvalue->value;
		printf("\n\tDBR_TIME_CHAR %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %lx %lx ",pvalue->stamp.secPastEpoch,pvalue->stamp.nsec);
		for (i = 0; i < count; i++,pchar++){
			printf("%d ",(short)(*pchar));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_TIME_LONG):
	{
		register struct dbr_time_long *pvalue
		  = (struct dbr_time_long *)pbuffer;
		register long *plong = &pvalue->value;
		printf("\n\tDBR_TIME_LONG %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %lx %lx ",pvalue->stamp.secPastEpoch,pvalue->stamp.nsec);
		for (i = 0; i < count; i++,plong++){
			printf("0x%lx ",*plong);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_TIME_DOUBLE):
	{
		register struct dbr_time_double *pvalue
		  = (struct dbr_time_double *)pbuffer;
		register double *pdouble = &pvalue->value;
		printf("\n\tDBR_TIME_DOUBLE %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %lx %lx ",pvalue->stamp.secPastEpoch,pvalue->stamp.nsec);
		for (i = 0; i < count; i++,pdouble++){
			printf("%6.4f ",(float)(*pdouble));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_GR_SHORT):
	{
		register struct dbr_gr_short *pvalue
		  = (struct dbr_gr_short *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("\n\tDBR_GR_SHORT %2d %2d",pvalue->status,pvalue->severity);
		printf(" %.8s %6d %6d %6d %6d %6d %6d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\n\t");
		for (i = 0; i < count; i++,pshort++){
			printf("%d ",*pshort);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_GR_FLOAT):
	{
		register struct dbr_gr_float *pvalue
		  = (struct dbr_gr_float *)pbuffer;
		register float *pfloat = &pvalue->value;
		printf("\n\tDBR_GR_FLOAT%2d %2d",pvalue->status,pvalue->severity);
		printf(" %3d %.8s\n\t%6.4f %6.4f %6.4f %6.4f %6.4f %6.4f",
		  pvalue->precision, pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\n\t");
		for (i = 0; i < count; i++,pfloat++){
			printf("%6.4f ",*pfloat);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_GR_ENUM):
	case (DBR_CTRL_ENUM):
	{
		register struct dbr_gr_enum *pvalue
		  = (struct dbr_gr_enum *)pbuffer;
		printf("\n\tDBR_GR_ENUM%2d %2d",pvalue->status,pvalue->severity);
		printf("\n\t%3d",pvalue->no_str);
		for (i = 0; i < pvalue->no_str; i++)
			printf("\n\t%.26s",pvalue->strs[i]);
		printf("\n\tValue: %d",pvalue->value);
		break;
	}
	case (DBR_GR_CHAR):
	{
		register struct dbr_gr_char *pvalue
		  = (struct dbr_gr_char *)pbuffer;
		register unsigned char *pchar = &pvalue->value;
		printf("\n\tDBR_GR_CHAR%2d %2d",pvalue->status,pvalue->severity);
		printf(" %.8s %4d %4d %4d %4d %4d %4d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			printf("%d ",(short)(*pchar));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_GR_LONG):
	{
		register struct dbr_gr_long *pvalue
		  = (struct dbr_gr_long *)pbuffer;
		register long *plong = &pvalue->value;
		printf("\n\tDBR_GR_LONG %2d %2d",pvalue->status,pvalue->severity);
		printf("%.8s\n\t%8d %8d %8d %8d %8d %8d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\n\t");
		for (i = 0; i < count; i++,plong++){
			printf("0x%lx ",*plong);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_GR_DOUBLE):
	{
		register struct dbr_gr_double *pvalue
		  = (struct dbr_gr_double *)pbuffer;
		register double *pdouble = &pvalue->value;
		printf("\n\tDBR_GR_DOUBLE %2d %2d",pvalue->status,pvalue->severity);
		printf(" %3d %.8s\n\t%6.4f %6.4f %6.4f %6.4f %6.4f %6.4f",
		  pvalue->precision, pvalue->units,
		  (float)(pvalue->upper_disp_limit),(float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),(float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),(float)(pvalue->lower_alarm_limit));
		printf("\n\t");
		for (i = 0; i < count; i++,pdouble++){
			printf("%6.4f ",(float)(*pdouble));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_CTRL_SHORT):
	{
		register struct dbr_ctrl_short *pvalue
		  = (struct dbr_ctrl_short *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("\n\tDBR_CTRL_SHORT %2d %2d",pvalue->status,pvalue->severity);
		printf(" %.8s\n\t%6d %6d %6d %6d %6d %6d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %6d %6d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\t");
		for (i = 0; i < count; i++,pshort++){
			printf("%d ",*pshort);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_CTRL_FLOAT):
	{
		register struct dbr_ctrl_float *pvalue
		  = (struct dbr_ctrl_float *)pbuffer;
		register float *pfloat = &pvalue->value;
		printf("\n\tDBR_CTRL_FLOAT %2d %2d",pvalue->status,pvalue->severity);
		printf(" %3d %.8s\n\t%6.4f %6.4f %6.4f %6.4f %6.4f %6.4f",
		  pvalue->precision, pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %6.4f %6.4f",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\t");
		for (i = 0; i < count; i++,pfloat++){
			printf("%6.4f ",*pfloat);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_CTRL_CHAR):
	{
		register struct dbr_ctrl_char *pvalue
		  = (struct dbr_ctrl_char *)pbuffer;
		register unsigned char *pchar = &pvalue->value;
		printf("\n\tDBR_CTRL_CHAR %2d %2d",pvalue->status,pvalue->severity);
		printf(" %.8s %4d %4d %4d %4d %4d %4d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %4d %4d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\t");
		for (i = 0; i < count; i++,pchar++){
			printf("%4d ",(short)(*pchar));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_CTRL_LONG):
	{
		register struct dbr_ctrl_long *pvalue
		  = (struct dbr_ctrl_long *)pbuffer;
		register long *plong = &pvalue->value;
		printf("\n\tDBR_CTRL_LONG %2d %2d",pvalue->status,pvalue->severity);
		printf(" %.8s %8d %8d\n\t%8d %8d %8d %8d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\t");
		for (i = 0; i < count; i++,plong++){
			printf("0x%lx ",*plong);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_CTRL_DOUBLE):
	{
		register struct dbr_ctrl_double *pvalue
		  = (struct dbr_ctrl_double *)pbuffer;
		register double *pdouble = &pvalue->value;
		printf("\n\tDBR_CTRL_DOUBLE%2d %2d",pvalue->status,pvalue->severity);
		printf(" %3d %.8s\n\t%6.6f %6.6f %6.6f %6.6f %6.6f %6.6f",
		  pvalue->precision, pvalue->units,
		  (float)(pvalue->upper_disp_limit),(float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),(float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),(float)(pvalue->lower_alarm_limit));
		printf(" %6.6f %6.6f",
		  (float)(pvalue->upper_ctrl_limit),(float)(pvalue->lower_ctrl_limit));
		printf("\n\t");
		for (i = 0; i < count; i++,pdouble++){
			printf("%6.6f ",(float)(*pdouble));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
    }
}

