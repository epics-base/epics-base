/* share/src/db  @(#)db_test.c	1.10     2/3/94 */
/*      database access subroutines */
/*
 *      Author: Bob Dalesio
 *      Date:   4/15/88
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
 * .01  07-26-91	mrk	Major cleanup
 * .02  08-06-91	mrk	More cleanup
 * .03	08-13-91	mrk	Added extra null arg to db_get_field calls
 * .04	02-02-94	mrk	added tpn (test put notify)
 * .05	02-03-94	mrk	gft was overrunning its buffer for arrays
 */
#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include        "dbDefs.h"
#include        "errlog.h"
#include        "db_access.h"


/* function declarations */
static void print_returned();


#define		MAX_ELEMS	10
int gft(char *pname)
{
	char		tgf_buffer[MAX_ELEMS*MAX_STRING_SIZE+sizeof(struct dbr_ctrl_double)];
	struct  dbAddr	addr;
	struct  dbAddr	*paddr = &addr;
	short		number_elements;
	int	i;
	int status;

	if (pname==0 || ((*pname < ' ') || (*pname > 'z'))) {
		printf("\nusage \"pv name\"\n");
		return(1);
	}
	/* convert name to database address */
	status=db_name_to_addr(pname,&addr);
	if(status) {
		printf("db_name_to_addr failed\n");
		return(1);
	}
	printf("   Record Name: %s\n",pname);
	printf("Record Address: 0x%p\n",addr.precord);
	printf("    Field Type: %d\n",addr.dbr_field_type);
	printf(" Field Address: 0x%p\n",addr.pfield);
	printf("    Field Size: %d\n",addr.field_size);
	printf("   No Elements: %ld\n",addr.no_elements);
	number_elements =
		((addr.no_elements > MAX_ELEMS)?MAX_ELEMS:addr.no_elements);

	for(i=0; i<=LAST_BUFFER_TYPE; i++) {
		if(addr.dbr_field_type==0) {
			if( (i!=DBR_STRING)
			 && (i!=DBR_STS_STRING)
			 && (i!=DBR_TIME_STRING)
			 && (i!=DBR_GR_STRING)
			 && (i!=DBR_CTRL_STRING)) continue;
		}
		if(db_get_field(paddr,i,tgf_buffer,number_elements,NULL)<0)
			printf("\t%s Failed\n",dbr_text[i]);
		else
			print_returned(i,tgf_buffer,number_elements);
	}
	return(0);
}

/*
 * TPF
 * Test put field
 */
int pft(char *pname,char *pvalue)
{
	struct dbAddr		addr;
	struct dbAddr		*paddr = &addr;
	char			buffer[500];
	short			shortvalue;
	long			longvalue;
	float			floatvalue;
	unsigned char		charvalue;
	double			doublevalue;
	int status;

	if (pname==0 || pvalue==0
	  || ((*pname < ' ') || (*pname > 'z'))
	  || ((*pvalue < ' ') || (*pvalue > 'z'))){
		printf("\nusage \"pv name\",\"value\"\n");
		return(1);
	}

	/* convert name to database address */

	/* convert name to database address */
	status=db_name_to_addr(pname,&addr);
	if(status) {
		printf("db_name_to_addr failed\n");
		return(1);
	}
	printf("   Record Name: %s\n",pname);
	printf("Record Address: 0x%p\n",addr.precord);
	printf("    Field Type: %d\n",addr.dbr_field_type);
	printf(" Field Address: 0x%p\n",addr.pfield);
	printf("    Field Size: %d\n",addr.field_size);
	printf("   No Elements: %ld\n",addr.no_elements);
	if (db_put_field(paddr,DBR_STRING,pvalue,1) < 0) printf("\n\t failed ");
	if (db_get_field(paddr,DBR_STRING,buffer,1,NULL) < 0) printf("\n\tfailed");
	else print_returned(DBR_STRING,buffer,1);
	if(addr.dbr_field_type<=DBF_STRING || addr.dbr_field_type==DBF_ENUM)
	  return(0);
	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
	  if (db_put_field(paddr,DBR_SHORT,&shortvalue,1) < 0) 
		printf("\n\t SHORT failed ");
	  if (db_get_field(paddr,DBR_SHORT,buffer,1,NULL) < 0) 
		printf("\n\t SHORT GET failed");
	  else print_returned(DBR_SHORT,buffer,1);
	}
	if(sscanf(pvalue,"%ld",&longvalue)==1) {
	  if (db_put_field(paddr,DBR_LONG,&longvalue,1) < 0) 
		printf("\n\t LONG failed ");
		if (db_get_field(paddr,DBR_LONG,buffer,1,NULL) < 0) 
		  printf("\n\t LONG GET failed");
		else print_returned(DBR_LONG,buffer,1);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
	  if (db_put_field(paddr,DBR_FLOAT,&floatvalue,1) < 0) 
		printf("\n\t FLOAT failed ");
	  if (db_get_field(paddr,DBR_FLOAT,buffer,1,NULL) < 0) 
		printf("\n\t FLOAT GET failed");
	  else print_returned(DBR_FLOAT,buffer,1);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
	  doublevalue=floatvalue;
	  if (db_put_field(paddr,DBR_DOUBLE,&doublevalue,1) < 0)
			printf("\n\t DOUBLE failed ");
	  if (db_get_field(paddr,DBR_DOUBLE,buffer,1,NULL) < 0) 
		printf("\n\t DOUBLE GET failed");
	  else print_returned(DBR_DOUBLE,buffer,1);
	}
	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
	  charvalue=(unsigned char)shortvalue;
	  if (db_put_field(paddr,DBR_CHAR,&charvalue,1) < 0) 
		printf("\n\t CHAR failed ");
	  if (db_get_field(paddr,DBR_CHAR,buffer,1,NULL) < 0) 
		printf("\n\t CHAR GET failed");
	  else print_returned(DBR_CHAR,buffer,1);
	}
	if(sscanf(pvalue,"%hu",&shortvalue)==1) {
	  if (db_put_field(paddr,DBR_ENUM,&shortvalue,1) < 0) 
		printf("\n\t ENUM failed ");
	  if (db_get_field(paddr,DBR_ENUM,buffer,1,NULL) < 0) 
		printf("\n\t ENUM GET failed");
	  else print_returned(DBR_ENUM,buffer,1);
	}
	printf("\n");
	return(0);
}

/*
 * PRINT_RETURNED
 *
 * print out the values in a database access interface structure
 */
static void print_returned(type,pbuffer,count)
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
			printf("%ld ", (unsigned long)*pvalue);
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
		dbr_long_t *plong = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*plong);
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
		printf("\tTimeStamp: %u %u",
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
		printf("\tTimeStamp: %u %u",
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
		printf("\tTimeStamp: %u %u",
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
		printf("\tTimeStamp: %u %u",
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
		dbr_long_t *plong = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %u %u",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*plong);
		}
		break;
	}
	case (DBR_TIME_DOUBLE):
	{
		struct dbr_time_double *pvalue
		  = (struct dbr_time_double *)pbuffer;
		double *pdouble = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %u %u",
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
		dbr_long_t *plong = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*plong);
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
		dbr_long_t *plong = &pvalue->value;
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
			printf("%d ",*plong);
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
	case (DBR_STSACK_STRING):
	{
		struct dbr_stsack_string *pvalue
		  = (struct dbr_stsack_string *)pbuffer;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf(" %2d %2d",pvalue->ackt,pvalue->acks);
		printf(" %s",pvalue->value);
		break;
	}
	case (DBR_CLASS_NAME):
	{
		printf(" %s",(char *)pbuffer);
		break;
	}
    }
    printf("\n");
}

static void tpnCallback(PUTNOTIFY *ppn)
{
    struct dbAddr	*pdbaddr = (struct dbAddr *)ppn->paddr;
    long	status = ppn->status;

    if(status==0)
	printf("tpnCallback: success record=%s\n",ppn->paddr->precord);
    else
	recGblRecordError(status,pdbaddr->precord,"tpnCallback");
    free((void *)pdbaddr);
    free(ppn);
}

long tpn(char	*pname,char *pvalue)
{
    long		status;
    struct dbAddr	*pdbaddr=NULL;
    PUTNOTIFY		*ppn=NULL;
    char		*psavevalue;
    int			len;

    len = strlen(pvalue);
    /*allocate space for value immediately following DBADDR*/
    pdbaddr = calloc(1,sizeof(struct dbAddr) + len+1);
    if(!pdbaddr) {
	printf("calloc failed\n");
	return(-1);
    }
    psavevalue = (char *)(pdbaddr + 1);
    strcpy(psavevalue,pvalue);
    status = db_name_to_addr(pname,pdbaddr);
    if(status) {
	printf("db_name_to_addr failed\n");
	free((void *)pdbaddr);
	return(-1);
    }
    ppn = calloc(1,sizeof(PUTNOTIFY));
    if(!pdbaddr) {
	printf("calloc failed\n");
	return(-1);
    }
    ppn->paddr = pdbaddr;
    ppn->pbuffer = psavevalue;
    ppn->nRequest = 1;
    if(dbPutNotifyMapType(ppn,DBR_STRING)) {
	printf("dbPutNotifyMapType failed\n");
	printf("calloc failed\n");
	return(-1);
    }
    ppn->userCallback = tpnCallback;
    status = dbPutNotify(ppn);
    if(status) {
    	errMessage(status, "tpn");
    }
    return(0);
}
