/*  dbl.c
 * dbl is a simple unix tool dumps various record names in a database
 *
 *      Original Author: Ben-chin Cha
 *      Date:            5-12-93
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
 * .01  mm-dd-yy        xxx     Comment
*/
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include	<cvtFast.h>
#include	<dbStaticLib.h>

#define MAX_TYPE		50
#define MAX_DEV_NUM		1000
#define MAX_DEV_NAME_SIZE	30
#define MAX_REC_NAME_SIZE	15

int 	DBL_OPTION;
int 	NUM_DEV_ITEMS,NUM_LIST_ITEMS;
char 	dbType[MAX_TYPE][MAX_REC_NAME_SIZE];
int 	rc_type[MAX_DEV_NUM];
char 	pvName[MAX_DEV_NUM][MAX_DEV_NAME_SIZE+1];
char 	dbl_option[MAX_REC_NAME_SIZE];
char 	dbl_search[MAX_DEV_NAME_SIZE];

int dbl_dbGetRecNames();

main(argc,argv)
int argc;
char **argv;
{
int i;
	dbl_option[0] = '\0';
	dbl_search[0] = '\0';
	if (argc == 1) {
		printf("USAGE: dbl <any.database> [option] [subString]\n\n");
		return(0);
		}

	if (argc >= 3) strcpy(dbl_option,argv[2]);
	if (argc == 4) strcpy(dbl_search,argv[3]);

	if (strcmp(argv[1],"help") == 0 || strcmp(argv[1],"-help") == 0
		|| strcmp(argv[1],"-h") == 0)
		{
		printf("\nUSAGE: dbl <any.database> [option] [subString]\n\n");
		printf("dbl - An EPICS database list utility, can be used to dump \n");
		printf("      the process variable names for the specified database\n\n");
		printf("where option can be set as following\n\n");
		printf("      1    dumps device type and the process variable name in the database\n");
		printf("      2    dumps database process variable names which contains the \"subString\" \n");
		printf("           ( where \"subString\" can be any alphabetic string )\n");
		printf("     ai    dumps database process variable names which are \"ai\" type \n");
		printf("     -1    dumps the header file for currently supported record types\n");
		exit(0);
		}

	 NUM_LIST_ITEMS = read_database(argv[1]);


}


int read_database(filename)
char *filename;
{
	DBBASE *pdbbase;
	DBENTRY *pdbentry;
	int	i,ntype;
	long	status;
	char	fname[80];
	char *str;
	FILE *fp;

	fp = fopen(filename,"r");
	if (!fp) {
                printf("ERROR: Database file '%s' not found\n\n",filename);
                return(0);
                }
	
	/* load data base record name to pvName */
	str = pvName[0];
	bzero(str,1000*31);

	pdbbase=dbAllocBase();
	pdbentry=dbAllocEntry(pdbbase);
	status=dbRead(pdbbase,fp);
	if(status) errMessage(status,"dbRead");
	fclose(fp);
if (strcmp(dbl_option,"-1") == 0) 
	ntype = dbl_dbGetRecTypes(pdbbase);

else {
	if (strlen(dbl_option) > 1) DBL_OPTION = 2;
	else if (strcmp(dbl_option,"1") == 0) DBL_OPTION = 1; 
	else if (strcmp(dbl_option,"2") == 0) DBL_OPTION = 3; 
	else DBL_OPTION = 0;
 
switch (DBL_OPTION) {
	case 0:  			/* dump pv name only */
	NUM_LIST_ITEMS = dbl_dbGetRecNames(pdbbase,NULL);
		for (i=0;i<NUM_LIST_ITEMS;i++)
		printf("%s\n",pvName[i]);
		break;

	case 1:				/* dump type & pv name */
	NUM_LIST_ITEMS = dbl_dbGetRecNames(pdbbase,NULL);
		for (i=0;i<NUM_LIST_ITEMS;i++)
		printf("%s\t\t %s\n",dbType[rc_type[i]],pvName[i]);
		break;

	case 2:				/* dump pv name of matched type */
	NUM_LIST_ITEMS = dbl_dbGetRecNames(pdbbase,dbl_option);
		for (i=0;i<NUM_LIST_ITEMS;i++)
		printf("%s\n",pvName[i]);
		break;

	case 3:				/* dump pv name of matched string */
	NUM_LIST_ITEMS = dbl_dbGetRecNames(pdbbase,NULL);
		for (i=0;i<NUM_LIST_ITEMS;i++) {
		if (strstr(pvName[i],dbl_search)) 
		printf("%s\n",pvName[i]);
		}
		break;
	}
   }

	dbFreeBase(pdbbase);

	NUM_DEV_ITEMS = NUM_LIST_ITEMS;
	return(NUM_DEV_ITEMS);
}

int dbl_dbGetRecNames(pdbbase,precdesname)
DBBASE *pdbbase;
char *precdesname;
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    int 	i=0,num=0;

    dbInitEntry(pdbbase,pdbentry);
    if(!precdesname)
	status = dbFirstRecdes(pdbentry);
    else
	status = dbFindRecdes(pdbentry,precdesname);
    if(status) {printf("No record description\n"); return(0);}
    while(!status) {
	strcpy(dbType[i],  dbGetRecdesName(pdbentry));
	status = dbFirstRecord(pdbentry);
	while(!status) {
	    strcpy(pvName[num],dbGetRecordName(pdbentry));
	    rc_type[num] = i;
            num++;
	    status = dbNextRecord(pdbentry);
	}
	if(precdesname) break;
	i++;
	status = dbNextRecdes(pdbentry);
    }
    dbFinishEntry(pdbentry);
    return(num);
}


int dbl_dbGetRecTypes(pdbbase)
DBBASE *pdbbase;
{
    DBENTRY	dbentry;
    DBENTRY	*pdbentry=&dbentry;
    long	status;
    int 	i=0,num=0;

    dbInitEntry(pdbbase,pdbentry);
	status = dbFirstRecdes(pdbentry);
    if(status) {printf("No record description\n"); return(0);}
    while(!status) {
	strcpy(dbType[i],  dbGetRecdesName(pdbentry));
	i++;
	status = dbNextRecdes(pdbentry);
    }
    dbFinishEntry(pdbentry);
	num = i;

printf("int NUM_REC_TYPE = %d;\n",num);
printf("char *dbType[] = {\n");
	for (i=0;i<num-1;i++) printf("\t\"%s\",\n",dbType[i]);
printf("\t\"%s\"\n",dbType[num-1]);
printf("\t};\n\n"); 

    return(i);
}
