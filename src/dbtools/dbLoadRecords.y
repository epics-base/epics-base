%{

/**************************************************************************
 *
 *     Author:	Jim Kowalkowski
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
 * .01	10-29-93	jbk	initial version
 *
 ***********************************************************************/

#include <stdio.h>
#include <string.h>

#ifdef vxWorks
#include <memLib.h>
#else
#include <memory.h>
#include <malloc.h>
#endif

#include "dbVarSub.h"
#include <dbStaticLib.h>
#include <epicsVersion.h>

static char subst_buffer[VAR_MAX_SUB_SIZE];
static int subst_used;
static int line_num;

static int yyerror();
static void sub_pvname(char*,char*);

#ifdef vxWorks
static DBENTRY* pdbentry;
extern struct dbBase *pdbBase;
#endif

%}

%start database

%token <Str> COMMA
%token <Str> WORD VALUE 
%token <Str> FIELD
%left O_BRACE C_BRACE O_PAREN C_PAREN
%left DATABASE CONTAINER RECORD
%left NOWHERE 

%union
{
    int	Int;
	char Char;
	char *Str;
	double Real;
}

%%

database:	DATABASE d_head d_body
	{
#ifdef vxWorks
		dbFreeEntry(pdbentry);
#endif
	}
	| DATABASE d_head /* jbk added for graphical thing */
	{
#ifdef vxWorks
		dbFreeEntry(pdbentry);
#endif
	}
	;

d_head:	O_PAREN WORD C_PAREN
	{
#ifdef vxWorks
		/*
		fprintf(stderr,"Warning: No EPICS version information in db file\n");
		*/
		pdbentry=dbAllocEntry(pdbBase);
		free($2);
#endif
	}
	| O_PAREN WORD COMMA VALUE C_PAREN
	{
#ifdef vxWorks
		int version,revision;
		char* v;

		v=strtok($4," ."); sscanf(v,"%d",&version);
		v=strtok(NULL," ."); sscanf(v,"%d",&revision);

		if(version!=EPICS_VERSION || revision!=EPICS_REVISION)
			fprintf(stderr,"Warning: Database not created with same version\n");

		pdbentry=dbAllocEntry(pdbBase);
		free($2); free($4);
#endif
	}
	;

d_body:	O_BRACE nowhere_records db_components C_BRACE
	;

/* nowhere is here for back compatability */

nowhere_records: /* null */
	| NOWHERE n_head n_body
	;

n_head: O_PAREN C_PAREN
	;

n_body: O_BRACE records C_BRACE
	;

db_components: /* null */
	| db_components container
	| db_components record
	;

container: CONTAINER c_head c_body
	;

c_head: O_PAREN WORD C_PAREN
	{
		free($2);
	}
	;

c_body: O_BRACE db_components C_BRACE
	;

records: /* null */
	| records record
	;

record:	RECORD r_head r_body
	{
#ifndef vxWorks
		printf(" }\n");
#endif
	}
	;

r_head:	O_PAREN WORD COMMA WORD C_PAREN
	{
		sub_pvname($2,$4);
		free($2); free($4);
	}
	| O_PAREN WORD COMMA VALUE C_PAREN
	{
		sub_pvname($2,$4);
		free($2); free($4);
	}
	;

r_body:	/* null */
	| O_BRACE fields C_BRACE
	;

fields: /* null */
	| fields field
	;

field:	FIELD O_PAREN WORD COMMA VALUE C_PAREN
	{
#ifdef vxWorks
		if( dbFindField(pdbentry,$<Str>3) )
			fprintf(stderr,"Cannot find field %s\n",$<Str>3);
#endif
		if(subst_used)
		{
			strcpy(subst_buffer,$<Str>5);
			if(dbDoSubst(subst_buffer,sizeof(subst_buffer),NULL)!=0)
				fprintf(stderr,"dbDoSubst failed\n");
#ifdef vxWorks
			if( dbPutString(pdbentry, subst_buffer) )
				fprintf(stderr,"Cannot set field %s to %s\n",
					$<Str>3,subst_buffer);
#else
			printf("\n\t\tfield(%s, \"%s\")",$<Str>3,subst_buffer);
#endif
		}
		else
		{
#ifdef vxWorks
			if( dbPutString(pdbentry, $<Str>5) )
				fprintf(stderr,"Cannot set field %s to %s\n",$<Str>3,$<Str>5);
#else
			printf("\n\t\tfield(%s, \"%s\")",$<Str>3,$<Str>5);
#endif
		}
		free($3); free($5);
	}
	;

%%
 
#include "dbLoadRecords_lex.c"
 
static int yyerror(str)
char  *str;
{ fprintf(stderr,"db file parse, Error line %d : %s\n",line_num, yytext); }
 
static int is_not_inited = 1;
 
int dbLoadRecords(char* pfilename, char* pattern, char* container)
{
	FILE* fp;
	long status;

#ifdef vxWorks
	if(pdbBase==NULL)
	{
		fprintf(stderr,"dbLoadRecords: default.dctsdr file not loaded\n");
		return -1;
	}
#endif

	if( pattern && *pattern )
	{
		subst_used = 1;
		dbInitSubst(pattern);
	}
	else
		subst_used = 0;

	if( !(fp=fopen(pfilename,"r")) )
	{
		fprintf(stderr,"dbLoadRecords: error opening file\n");
		return -1;
	}

#ifndef vxWorks
	/* if(container) printf(" %s {\n",container); */
#endif

	if(is_not_inited)
	{
#ifdef ERROR_STUFF
		fprintf(stderr,"initing parser\n");
#endif
		yyin=fp;
		is_not_inited=0;
	}
	else
	{
#ifdef ERROR_STUFF
		fprintf(stderr,"restarting parser\n");
#endif
		yyrestart(fp);
	}
#ifdef ERROR_STUFF
		fprintf(stderr,"before parser startup\n");
#endif
	yyparse();

#ifndef vxWorks
	/* if(container) printf(" }\n"); */
#endif

	if(subst_used) dbFreeSubst();

	fclose(fp);
	return 0;
}

static void sub_pvname(char* type, char* name)
{
#ifdef vxWorks
		if( dbFindRecdes(pdbentry,type) )
			fprintf(stderr,"Cannot find record type %s\n",type);
#endif

		if(subst_used)
		{
			strcpy(subst_buffer,name);
			if(dbDoSubst(subst_buffer,PVNAME_STRINGSZ,NULL)!=0)
				fprintf(stderr,"dbDoSubst failed\n");
#ifdef vxWorks
			if( dbCreateRecord(pdbentry,subst_buffer) )
				fprintf(stderr,"Cannot create record %s\n",subst_buffer);
#else
			printf("\trecord(%s, \"%s\") {",type,subst_buffer);
#endif
		}
		else
		{
#ifdef vxWorks
			if( dbCreateRecord(pdbentry,name) )
				fprintf(stderr,"Cannot create record %s\n",name);
#else
			printf("\trecord(%s, \"%s\") {",type,name);
#endif
		}
}

