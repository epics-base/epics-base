/*	@(#)ca_test.c	$Id$
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
 * .02	08-05-91	mrk	Make more compatible with db_test.c
 * .03	09-24-91	joh	changed declaration of `outstanding'
 *				to a long
 * .04	01-14-91	joh	documentation
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 */

#ifdef vxWorks
#include <vxWorks.h>
#endif

#ifndef ERROR
#define ERROR -1
#endif
#ifndef OK
#define OK 0
#endif

#include        <cadef.h>
#include        <string.h>

void 		printit();
void 		verify();

static long	outstanding;


/*
 *
 *	main
 *
 *	parse command line arguments
 */
#ifndef vxWorks
main(argc,argv)
int     argc;
char    **argv;
{

	/*
	 * print error and return if arguments are invalid
	 */
	if(argc < 2  || argc > 3){
		printf("usage: ca_test channel_name <\"put value\">\n");
		printf("the following arguments were received\n");
		while(argc>0) {
			printf("%s\n",argv[0]);
			argv++; argc--;
		}
		return ERROR;
	}


	/*
	 * check for supplied value
	 */
	if(argc == 2){
		return ca_test(argv[1], NULL);
	}
	else if(argc == 3){
		char *pt;

		/* strip leading and trailing quotes*/
		if(argv[2][1]=='"') argv[2]++;
		if(pt=strchr(argv[2],'"')) *pt = 0;
		return ca_test(argv[1], argv[2]);
	}
	else{
		return ERROR;
	}

}
#endif


/*
 *  	ca_test
 *
 *	find channel, write a value if supplied, and 
 *	read back the current value
 *
 */
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



/*
 * 	cagft()
 *
 *	ca get field test
 *
 *	test ca get over the range of CA data types
 */
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
		if(ca_field_type(chan_id)==0) {
			if( (i!=DBR_STRING)
			  && (i!=DBR_STS_STRING)
			  && (i!=DBR_TIME_STRING)
			  && (i!=DBR_GR_STRING)
			  && (i!=DBR_CTRL_STRING)) continue;
		}
		status = ca_array_get_callback(
				i, 
				ca_element_count(chan_id),
				chan_id, 
				printit, 
				NULL);
		SEVCHK(status, NULL);

		outstanding++;
	}

	/*
	 * wait for the operation to complete
	 * before returning 
	 */
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
 *	capft
 *
 *	test ca_put() over a range of data types
 *	
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

	/*
	 *  string value ca_put
	 */
	status = ca_put(
			DBR_STRING, 
			chan_id, 
			pvalue);
	SEVCHK(status, NULL);
	verify(chan_id, DBR_STRING);

	if(ca_field_type(chan_id)==0)goto skip_rest;

	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
		/*
		 * short integer ca_put
		 */
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
		/*
		 * long integer ca_put
		 */
		status = ca_put(
				DBR_LONG, 
				chan_id, 
				&longvalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_LONG);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
		/*
		 * single precision float ca_put
		 */
		status = ca_put(
				DBR_FLOAT, 
				chan_id, 
				&floatvalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_FLOAT);
	}
	if(sscanf(pvalue,"%lf",&doublevalue)==1) {
		/*
		 * double precision float ca_put
		 */
		status = ca_put(
				DBR_DOUBLE, 
				chan_id, 
				&doublevalue);
		SEVCHK(status, NULL);
		verify(chan_id, DBR_DOUBLE);
	}

skip_rest:

	/*
	 * wait for the operation to complete
	 * (outstabnding decrements to zero)
	 */
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
 * initiate print out the values in a database access interface structure
 */
static void verify(chan_id, type)
chid		chan_id;
int		type;
{
	int status;

	/*
	 * issue a get which calls back `printit'
	 * upon completion
	 */
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
 *
 * switches over the range of CA data types and reports the value
 */
static print_returned(type,pbuffer,count)
  short	type;
  char	*pbuffer;
  short		count;
{
    short i;

    printf("%s\t",dbr_text[type]);
    switch(type){
	case (DBR_STRING):
		for(i=0; i<count && *pbuffer!=0; i++) {
			if(count!=1 && (i%5 == 0)) printf("\n");
			printf("%s ",pbuffer);
			pbuffer += MAX_STRING_SIZE;
		}
		break;
	case (DBR_SHORT):
	case (DBR_ENUM):
	{
		short *pvalue = (short *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*(short *)pvalue);
		}
		break;
	}
	case (DBR_FLOAT):
	{
		float *pvalue = (float *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*(float *)pvalue);
		}
		break;
	}
	case (DBR_CHAR):
	{
		int	value;

		for (i = 0; i < count; i++,pbuffer++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			value = *(unsigned char *)pbuffer;
			printf("%d ",value);
		}
		break;
	}
	case (DBR_LONG):
	{
		long *pvalue = (long *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("0x%x ",*pvalue);
		}
		break;
	}
	case (DBR_DOUBLE):
	{
		double *pvalue = (double *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pvalue));
		}
		break;
	}
	case (DBR_STS_STRING):
	case (DBR_GR_STRING):
	case (DBR_CTRL_STRING):
	{
		struct dbr_sts_string *pvalue 
		  = (struct dbr_sts_string *) pbuffer;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %s",pvalue->value);
		break;
	}
	case (DBR_STS_ENUM):
	case (DBR_STS_SHORT):
	{
		struct dbr_sts_short *pvalue
		  = (struct dbr_sts_short *)pbuffer;
		short *pshort = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_STS_FLOAT):
	{
		struct dbr_sts_float *pvalue
		  = (struct dbr_sts_float *)pbuffer;
		float *pfloat = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_STS_CHAR):
	{
		struct dbr_sts_char *pvalue
		  = (struct dbr_sts_char *)pbuffer;
		unsigned char *pchar = &pvalue->value;
		short value;

		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			value=*pchar;
			printf("%d ",value);
		}
		break;
	}
	case (DBR_STS_LONG):
	{
		struct dbr_sts_long *pvalue
		  = (struct dbr_sts_long *)pbuffer;
		long *plong = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("0x%lx ",*plong);
		}
		break;
	}
	case (DBR_STS_DOUBLE):
	{
		struct dbr_sts_double *pvalue
		  = (struct dbr_sts_double *)pbuffer;
		double *pdouble = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pdouble));
		}
		break;
	}
	case (DBR_TIME_STRING):
	{
		struct dbr_time_string *pvalue 
		  = (struct dbr_time_string *) pbuffer;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %lx %lx",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		printf("\tValue: ");
		printf("%s",pvalue->value);
		break;
	}
	case (DBR_TIME_SHORT):
	case (DBR_TIME_ENUM):
	{
		struct dbr_time_short *pvalue
		  = (struct dbr_time_short *)pbuffer;
		short *pshort = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %lx %lx",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_TIME_FLOAT):
	{
		struct dbr_time_float *pvalue
		  = (struct dbr_time_float *)pbuffer;
		float *pfloat = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %lx %lx",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_TIME_CHAR):
	{
		struct dbr_time_char *pvalue
		  = (struct dbr_time_char *)pbuffer;
		unsigned char *pchar = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %lx %lx",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",(short)(*pchar));
		}
		break;
	}
	case (DBR_TIME_LONG):
	{
		struct dbr_time_long *pvalue
		  = (struct dbr_time_long *)pbuffer;
		long *plong = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %lx %lx",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("0x%lx ",*plong);
		}
		break;
	}
	case (DBR_TIME_DOUBLE):
	{
		struct dbr_time_double *pvalue
		  = (struct dbr_time_double *)pbuffer;
		double *pdouble = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %lx %lx",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pdouble));
		}
		break;
	}
	case (DBR_GR_SHORT):
	{
		struct dbr_gr_short *pvalue
		  = (struct dbr_gr_short *)pbuffer;
		short *pshort = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_GR_FLOAT):
	{
		struct dbr_gr_float *pvalue
		  = (struct dbr_gr_float *)pbuffer;
		float *pfloat = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_GR_ENUM):
	case (DBR_CTRL_ENUM):
	{
		struct dbr_gr_enum *pvalue
		  = (struct dbr_gr_enum *)pbuffer;
		printf("%2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tValue: %d",pvalue->value);
		if(pvalue->no_str>0) {
			printf("\n\t%3d",pvalue->no_str);
			for (i = 0; i < pvalue->no_str; i++)
				printf("\n\t%.26s",pvalue->strs[i]);
		}
		break;
	}
	case (DBR_GR_CHAR):
	{
		struct dbr_gr_char *pvalue
		  = (struct dbr_gr_char *)pbuffer;
		unsigned char *pchar = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",(short)(*pchar));
		}
		break;
	}
	case (DBR_GR_LONG):
	{
		struct dbr_gr_long *pvalue
		  = (struct dbr_gr_long *)pbuffer;
		long *plong = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("0x%lx ",*plong);
		}
		break;
	}
	case (DBR_GR_DOUBLE):
	{
		struct dbr_gr_double *pvalue
		  = (struct dbr_gr_double *)pbuffer;
		double *pdouble = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  (float)(pvalue->upper_disp_limit),
		  (float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),
		  (float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),
		  (float)(pvalue->lower_alarm_limit));
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pdouble));
		}
		break;
	}
	case (DBR_CTRL_SHORT):
	{
		struct dbr_ctrl_short *pvalue
		  = (struct dbr_ctrl_short *)pbuffer;
		short *pshort = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_CTRL_FLOAT):
	{
		struct dbr_ctrl_float *pvalue
		  = (struct dbr_ctrl_float *)pbuffer;
		float *pfloat = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8.3f %8.3f",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_CTRL_CHAR):
	{
		struct dbr_ctrl_char *pvalue
		  = (struct dbr_ctrl_char *)pbuffer;
		unsigned char *pchar = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%4d ",(short)(*pchar));
		}
		break;
	}
	case (DBR_CTRL_LONG):
	{
		struct dbr_ctrl_long *pvalue
		  = (struct dbr_ctrl_long *)pbuffer;
		long *plong = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("0x%lx ",*plong);
		}
		break;
	}
	case (DBR_CTRL_DOUBLE):
	{
		struct dbr_ctrl_double *pvalue
		  = (struct dbr_ctrl_double *)pbuffer;
		double *pdouble = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  (float)(pvalue->upper_disp_limit),
		  (float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),
		  (float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),
		  (float)(pvalue->lower_alarm_limit));
		printf(" %8.3f %8.3f",
		  (float)(pvalue->upper_ctrl_limit),
		  (float)(pvalue->lower_ctrl_limit));
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.6f ",(float)(*pdouble));
		}
		break;
	}
    }
    printf("\n");
}

