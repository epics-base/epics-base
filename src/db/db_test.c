/* share/src/db  $Id$ */

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
 * .01  mm-dd-yy        iii     Comment
 */

#include        <db_access.h>

/* function declarations */
static print_returned();


/*
 * TGF
 * Test get field
 */
char		tgf_buffer[1200];
#define		MAX_ELEMS	250
int gft(name,index)
char		*name;
register short	index;
{
	struct  db_addr	addr;
	struct  db_addr	*paddr = &addr;
	short		number_elements;

	/* convert name to database address */
	db_name_to_addr(name,&addr,index);
	
	printf("   Record Type: %d\n",addr.record_type);
	printf("Record Address: 0x%x\n",addr.precord);
	printf("    Field Type: %d\n",addr.field_type);
	printf(" Field Address: 0x%x\n",addr.pfield);
	printf("    Field Size: %d\n",addr.field_size);
	printf("   No Elements: %d\n",addr.no_elements);
	number_elements =
		((addr.no_elements > MAX_ELEMS)?MAX_ELEMS:addr.no_elements);

	/* failedfetch as each type */
	if (db_get_field(paddr,DBR_STRING,tgf_buffer,1) < 0)
	    printf("\n\tDBR_STRING failed");
	else print_returned(DBR_STRING,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_SHORT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_SHORT failed");
	else print_returned(DBR_SHORT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_FLOAT failed");
	else print_returned(DBR_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_ENUM,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_ENUM failed");
	else print_returned(DBR_ENUM,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CHAR,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_CHAR failed");
	else print_returned(DBR_CHAR,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_LONG,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_LONG failed");
	else print_returned(DBR_LONG,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_DOUBLE,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_DOUBLE failed");
	else print_returned(DBR_DOUBLE,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_STRING,tgf_buffer,1) < 0)
	    printf("\n\tDBR_STS_STRING failed");
	else print_returned(DBR_STS_STRING,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_SHORT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_STS_SHORT failed");
	else print_returned(DBR_STS_SHORT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_STS_FLOAT failed");
	else print_returned(DBR_STS_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_ENUM,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_STS_ENUM failed");
	else print_returned(DBR_STS_ENUM,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_CHAR,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_STS_CHAR failed");
	else print_returned(DBR_STS_CHAR,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_LONG,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_STS_LONG failed");
	else print_returned(DBR_STS_LONG,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_DOUBLE,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_STS_DOUBLE failed");
	else print_returned(DBR_STS_DOUBLE,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_TIME_STRING,tgf_buffer,1) < 0)
	    printf("\n\tDBR_TIME_STRING failed");
	else print_returned(DBR_TIME_STRING,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_TIME_SHORT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_TIME_SHORT failed");
	else print_returned(DBR_TIME_SHORT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_TIME_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_TIME_FLOAT failed");
	else print_returned(DBR_TIME_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_TIME_ENUM,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_TIME_ENUM failed");
	else print_returned(DBR_TIME_ENUM,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_TIME_CHAR,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_TIME_CHAR failed");
	else print_returned(DBR_TIME_CHAR,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_TIME_LONG,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_TIME_LONG failed");
	else print_returned(DBR_TIME_LONG,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_TIME_DOUBLE,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_TIME_DOUBLE failed");
	else print_returned(DBR_TIME_DOUBLE,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_STRING,tgf_buffer,1) < 0)
	    printf("\n\tDBR_GR_STRING failed");
	else print_returned(DBR_GR_STRING,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_SHORT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_GR_SHORT failed");
	else print_returned(DBR_GR_SHORT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_GR_FLOAT failed");
	else print_returned(DBR_GR_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_ENUM,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_GR_ENUM failed");
	else print_returned(DBR_GR_ENUM,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_CHAR,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_GR_CHAR failed");
	else print_returned(DBR_GR_CHAR,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_LONG,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_GR_LONG failed");
	else print_returned(DBR_GR_LONG,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_DOUBLE,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_GR_DOUBLE failed");
	else print_returned(DBR_GR_DOUBLE,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_SHORT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_CTRL_SHORT failed");
	else print_returned(DBR_CTRL_SHORT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_CTRL_FLOAT failed");
	else print_returned(DBR_CTRL_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_CHAR,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_CTRL_CHAR failed");
	else print_returned(DBR_CTRL_CHAR,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_LONG,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_CTRL_LONG failed");
	else print_returned(DBR_CTRL_LONG,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_DOUBLE,tgf_buffer,number_elements) < 0)
	    printf("\n\tDBR_CTRL_DOUBLE failed");
	else print_returned(DBR_CTRL_DOUBLE,tgf_buffer,number_elements);
	printf("\n");
	return(0);
}

/*
 * TPF
 * Test put field
 */
int pft(pname,pvalue,index)
char		*pname;
char		*pvalue;
register short	index;
{
	struct db_addr		addr;
	struct db_addr		*paddr = &addr;
	char			buffer[500];
	struct dbr_ctrl_enum	test_enum;
	register short		i;
	short			shortvalue;
	long			longvalue;
	float			floatvalue;
	unsigned char		charvalue;
	double			doublevalue;

	if (((*pname < ' ') || (*pname > 'z'))
	  || ((*pvalue < ' ') || (*pvalue > 'z'))){
		printf("\nusage \"pv name\",\"value\"\n");
		return;
	}

	/* convert name to database address */
	db_name_to_addr(pname,&addr,index);
	printf("   Record Type: %d\n",addr.record_type);
	printf("Record Address: 0x%x\n",addr.precord);
	printf("    Field Type: %d\n",addr.field_type);
	printf(" Field Address: 0x%x\n",addr.pfield);
	printf("    Field Size: %d\n",addr.field_size);
	printf("   No Elements: %d\n",addr.no_elements);
	if (db_put_field(paddr,DBR_STRING,pvalue,1) < 0) printf("\n\t failed ");
	if (db_get_field(paddr,DBR_STRING,buffer,1) < 0) printf("\n\tfailed");
	else print_returned(DBR_STRING,buffer,1);
	if(addr.field_type<=DBF_STRING || addr.field_type>=DBF_NO_ACCESS)
	  return(0);
	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
	  if (db_put_field(paddr,DBR_SHORT,&shortvalue,1) < 0) 
		printf("\n\t SHORT failed ");
	  if (db_get_field(paddr,DBR_SHORT,buffer,1) < 0) 
		printf("\n\t SHORT GET failed");
	  else print_returned(DBR_SHORT,buffer,1);
	}
	if(sscanf(pvalue,"%ld",&longvalue)==1) {
	  if (db_put_field(paddr,DBR_LONG,&longvalue,1) < 0) 
		printf("\n\t LONG failed ");
		if (db_get_field(paddr,DBR_LONG,buffer,1) < 0) 
		  printf("\n\t LONG GET failed");
		else print_returned(DBR_LONG,buffer,1);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
	  if (db_put_field(paddr,DBR_FLOAT,&floatvalue,1) < 0) 
		printf("\n\t FLOAT failed ");
	  if (db_get_field(paddr,DBR_FLOAT,buffer,1) < 0) 
		printf("\n\t FLOAT GET failed");
	  else print_returned(DBR_FLOAT,buffer,1);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
	  doublevalue=floatvalue;
	  if (db_put_field(paddr,DBR_DOUBLE,&doublevalue,1) < 0)
			printf("\n\t DOUBLE failed ");
	  if (db_get_field(paddr,DBR_DOUBLE,buffer,1) < 0) 
		printf("\n\t DOUBLE GET failed");
	  else print_returned(DBR_DOUBLE,buffer,1);
	}
	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
	  charvalue=(unsigned char)shortvalue;
	  if (db_put_field(paddr,DBR_CHAR,&charvalue,1) < 0) 
		printf("\n\t CHAR failed ");
	  if (db_get_field(paddr,DBR_CHAR,buffer,1) < 0) 
		printf("\n\t CHAR GET failed");
	  else print_returned(DBR_CHAR,buffer,1);
	}
	if(sscanf(pvalue,"%hu",&shortvalue)==1) {
	  if (db_put_field(paddr,DBR_ENUM,&shortvalue,1) < 0) 
		printf("\n\t ENUM failed ");
	  if (db_get_field(paddr,DBR_ENUM,buffer,1) < 0) 
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
		printf("\n\tDBR_STS_STRING %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tValue: %s",pvalue->value);
		break;
	}
	case (DBR_STS_SHORT):
	{
		register struct dbr_sts_short *pvalue
		  = (struct dbr_sts_short *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("\n\tDBR_STS_SHORT %2d %2d",pvalue->status,
			pvalue->severity);
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
		printf("\n\tDBR_STS_FLOAT %2d %2d",pvalue->status,
			pvalue->severity);
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
		printf("\n\tDBR_STS_ENUM %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tValue: %d",pvalue->value);
		break;
	}
	case (DBR_STS_CHAR):
	{
		register struct dbr_sts_char *pvalue
		  = (struct dbr_sts_char *)pbuffer;
		register unsigned char *pchar = &pvalue->value;
		short value;

		printf("\n\tDBR_STS_CHAR %2d %2d",pvalue->status,
			pvalue->severity);
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
		printf("\n\tDBR_STS_LONG %2d %2d",pvalue->status,
			pvalue->severity);
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
		printf("\n\tDBR_STS_DOUBLE %2d %2d",pvalue->status,
			pvalue->severity);
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
		printf("\n\tDBR_TIME_STRING %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\n\tTimeStamp: %lx %lx \n\tValue: ",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		printf("%s\n\t",pvalue->value);
		break;
	}
	case (DBR_TIME_SHORT):
	{
		register struct dbr_time_short *pvalue
		  = (struct dbr_time_short *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("\n\tDBR_TIME_SHORT %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tTimeStamp: %lx %lx \n\tValue: ",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
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
		printf("\n\tDBR_TIME_FLOAT %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tTimeStamp: %lx %lx \n\tValue: ",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
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
		printf("\n\tDBR_TIME_ENUM %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tTimeStamp: %lx %lx \n\tValue: ",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
		printf("%d\n\t",pvalue->value);
		break;
	}
	case (DBR_TIME_CHAR):
	{
		register struct dbr_time_char *pvalue
		  = (struct dbr_time_char *)pbuffer;
		register unsigned char *pchar = &pvalue->value;
		printf("\n\tDBR_TIME_CHAR %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tTimeStamp: %lx %lx \n\tValue: ",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
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
		printf("\n\tDBR_TIME_LONG %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tTimeStamp: %lx %lx \n\tValue: ",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
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
		printf("\n\tDBR_TIME_DOUBLE %2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tTimeStamp: %lx %lx \n\tValue: ",
			pvalue->stamp.secPastEpoch, pvalue->stamp.nsec);
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
		printf("\n\tDBR_GR_SHORT %2d %2d",pvalue->status,
			pvalue->severity);
		printf(" %.8s %6d %6d %6d %6d %6d %6d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_GR_FLOAT%2d %2d",pvalue->status,
			pvalue->severity);
		printf(" %3d %.8s\n\t%6.4f %6.4f %6.4f %6.4f %6.4f %6.4f",
		  pvalue->precision, pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_GR_ENUM%2d %2d",pvalue->status,
			pvalue->severity);
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
		printf("\n\tDBR_GR_CHAR%2d %2d",pvalue->status,
			pvalue->severity);
		printf(" %.8s %4d %4d %4d %4d %4d %4d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_GR_LONG %2d %2d",pvalue->status,
			pvalue->severity);
		printf("%.8s\n\t%8d %8d %8d %8d %8d %8d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_GR_DOUBLE %2d %2d",pvalue->status,
			pvalue->severity);
		printf(" %3d %.8s\n\t%6.4f %6.4f %6.4f %6.4f %6.4f %6.4f",
		  pvalue->precision, pvalue->units,
		  (float)(pvalue->upper_disp_limit),
		  (float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),
		  (float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),
		  (float)(pvalue->lower_alarm_limit));
		printf("\n\tValue: ");
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
		printf("\n\tDBR_CTRL_SHORT %2d %2d",pvalue->status,
		  pvalue->severity);
		printf(" %.8s\n\t%6d %6d %6d %6d %6d %6d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %6d %6d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_CTRL_FLOAT %2d %2d",pvalue->status,
		  pvalue->severity);
		printf(" %3d %.8s\n\t%6.4f %6.4f %6.4f %6.4f %6.4f %6.4f",
		  pvalue->precision, pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %6.4f %6.4f",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_CTRL_CHAR %2d %2d",pvalue->status,
		  pvalue->severity);
		printf(" %.8s %4d %4d %4d %4d %4d %4d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %4d %4d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_CTRL_LONG %2d %2d",pvalue->status,
		  pvalue->severity);
		printf(" %.8s %8d %8d\n\t%8d %8d %8d %8d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		printf("\n\tValue: ");
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
		printf("\n\tDBR_CTRL_DOUBLE%2d %2d",pvalue->status,
		  pvalue->severity);
		printf(" %3d %.8s\n\t%6.6f %6.6f %6.6f %6.6f %6.6f %6.6f",
		  pvalue->precision, pvalue->units,
		  (float)(pvalue->upper_disp_limit),
		  (float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),
		  (float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),
		  (float)(pvalue->lower_alarm_limit));
		printf(" %6.6f %6.6f",
		  (float)(pvalue->upper_ctrl_limit),
		  (float)(pvalue->lower_ctrl_limit));
		printf("\n\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			printf("%6.6f ",(float)(*pdouble));
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
    }
}

