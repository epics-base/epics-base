
/* db_test.c */
/* share/src/db $Id$ */

/*
 *
 *      database access subroutines
 *
 *      Author: Bob Dalesio
 *      Date:   4/15/88
 * @(#)iocdbaccess.c    1.1     9/22/88
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Bob Dalesio, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-3414
 *	E-mail: dalesio@luke.lanl.gov
 *
 * Modification Log:
 * -----------------
 */
#include        <db_access.h>

/*
 * TGF
 * Test get field
 */
char		tgf_buffer[1200];
#define		MAX_ELEMS	250
int tgf(name,index)
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
	printf("       Special: %d\n",addr.special);
	printf("    Choice Set: %d\n",addr.choice_set);
	number_elements =
		((addr.no_elements > MAX_ELEMS)?MAX_ELEMS:addr.no_elements);

	/* failedfetch as each type */
	if (db_get_field(paddr,DBR_STRING,tgf_buffer,1) < 0)
	    printf("DBR_STRING failed\n");
	else print_returned(DBR_STRING,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_INT,tgf_buffer,number_elements) < 0)
	    printf("DBR_INT failed\n");
	else print_returned(DBR_INT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("DBR_FLOAT failed\n");
	else print_returned(DBR_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_ENUM,tgf_buffer,number_elements) < 0)
	    printf("DBR_ENUM failed\n");
	else print_returned(DBR_ENUM,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_STRING,tgf_buffer,1) < 0)
	    printf("DBR_STS_STRING failed\n");
	else print_returned(DBR_STS_STRING,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_INT,tgf_buffer,number_elements) < 0)
	    printf("DBR_STS_INT failed\n");
	else print_returned(DBR_STS_INT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("DBR_STS_FLOAT failed\n");
	else print_returned(DBR_STS_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_STS_ENUM,tgf_buffer,number_elements) < 0)
	    printf("DBR_STS_ENUM failed\n");
	else print_returned(DBR_STS_ENUM,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_INT,tgf_buffer,number_elements) < 0)
	    printf("DBR_GR_INT failed\n");
	else print_returned(DBR_GR_INT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_GR_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("DBR_GR_FLOAT failed\n");
	else print_returned(DBR_GR_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_INT,tgf_buffer,number_elements) < 0)
	    printf("DBR_CTRL_INT failed\n");
	else print_returned(DBR_CTRL_INT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_FLOAT,tgf_buffer,number_elements) < 0)
	    printf("DBR_CTRL_FLOAT failed\n");
	else print_returned(DBR_CTRL_FLOAT,tgf_buffer,number_elements);
	if (db_get_field(paddr,DBR_CTRL_ENUM,tgf_buffer,number_elements) < 0)
	    printf("DBR_CTRL_ENUM failed\n");
	else print_returned(DBR_CTRL_ENUM,tgf_buffer,number_elements);
	printf("\n");
	return(0);
}

/*
 * TPF
 * Test put field
 */
int tpf(pname,pvalue,index)
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
	printf("       Special: %d\n",addr.special);
	printf("    Choice Set: %d\n",addr.choice_set);
	if (db_put_field(paddr,DBR_STRING,pvalue,1) < 0) printf(" failed\n");
	if (db_get_field(paddr,DBR_STRING,buffer,1) < 0) printf("failed\n");
	else print_returned(DBR_STRING,buffer,1);
	if(addr.field_type<=DBF_STRING || addr.field_type>=DBF_NO_ACCESS)return(0);
	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
		if (db_put_field(paddr,DBR_INT,&shortvalue,1) < 0) printf(" INT failed\n");
		if (db_get_field(paddr,DBR_INT,buffer,1) < 0) printf("INT GET failed\n");
		else print_returned(DBR_INT,buffer,1);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
		if (db_put_field(paddr,DBR_FLOAT,&floatvalue,1) < 0) printf("FLOAT failed\n");
		if (db_get_field(paddr,DBR_FLOAT,buffer,1) < 0) printf("FLOAT GET failed\n");
		else print_returned(DBR_FLOAT,buffer,1);
	}
	if(sscanf(pvalue,"%hu",&shortvalue)==1) {
		if (db_put_field(paddr,DBR_ENUM,&shortvalue,1) < 0) printf("ENUM failed\n");
		if (db_get_field(paddr,DBR_ENUM,buffer,1) < 0) printf("ENUM GET failed\n");
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
		printf("DBR_STRING: %s",pbuffer);
		break;
	case (DBR_INT):
	{
		register short *pvalue = (short *)pbuffer;
		printf("DBR_INT: ");
		for (i = 0; i < count; i++,pvalue++){
			printf("%d ",*(short *)pvalue);
			if ((i % 16) == 15) printf("\t\n");
		}
		break;
	}
	case (DBR_FLOAT):
	{
		register float *pvalue = (float *)pbuffer;
		printf("DBR_FLOAT: ");
		for (i = 0; i < count; i++,pvalue++){
			printf("%6.4f ",*(float *)pvalue);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_ENUM):
	{
		register short *pvalue = (short *)pbuffer;
		printf("DBR_ENUM: %d",*pvalue);
		break;
	}
	case (DBR_STS_STRING):
	{
		register struct dbr_sts_string *pvalue 
		  = (struct dbr_sts_string *) pbuffer;
		printf("DBR_STS_STRING %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %s",pvalue->value);
		break;
	}
	case (DBR_STS_INT):
	{
		register struct dbr_sts_int *pvalue
		  = (struct dbr_sts_int *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("DBR_STS_INT %2d %2d",pvalue->status,pvalue->severity);
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
		printf("DBR_STS_FLOAT %2d %2d",pvalue->status,pvalue->severity);
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
		printf("DBR_STS_ENUM %2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %d",pvalue->value);
		break;
	}
	case (DBR_GR_INT):
	{
		register struct dbr_gr_int *pvalue
		  = (struct dbr_gr_int *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("DBR_GR_INT %2d %2d",pvalue->status,pvalue->severity);
		printf(" %.8s %6d %6d %6d %6d %6d %6d",pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\nValue:\t");
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
		printf("DBR_GR_FLOAT%2d %2d",pvalue->status,pvalue->severity);
		printf(" %3d %.8s\n\t%6.4f %6.4f %6.4f %6.4f %6.4f %6.4f",
		  pvalue->precision, pvalue->units,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf("\nValue\t");
		for (i = 0; i < count; i++,pfloat++){
			printf("%6.4f ",*pfloat);
			if ((i % 16) == 15) printf("\n\t");
		}
		break;
	}
	case (DBR_CTRL_INT):
	{
		register struct dbr_ctrl_int *pvalue
		  = (struct dbr_ctrl_int *)pbuffer;
		register short *pshort = &pvalue->value;
		printf("DBR_CTRL_INT %2d %2d",pvalue->status,pvalue->severity);
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
		printf("DBR_CTRL_FLOAT %2d %2d",pvalue->status,pvalue->severity);
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
	case (DBR_CTRL_ENUM):
	{
		register struct dbr_ctrl_enum *pvalue
		  = (struct dbr_ctrl_enum *)pbuffer;
		printf("DBR_GR_ENUM%2d %2d",pvalue->status,pvalue->severity);
		printf("\n\t%3d",pvalue->no_str);
		for (i = 0; i < pvalue->no_str; i++)
			printf("\n\t%.26s",pvalue->strs[i]);
		printf("Value: %d",pvalue->value);
		break;
	}
    }
    printf("\n");
}

