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

struct db_app_node
{
	char* name;
	struct db_app_node* next;
};
typedef struct db_app_node DB_APP_NODE;

DB_APP_NODE* DbApplList=(DB_APP_NODE*)NULL;
static DB_APP_NODE* DbCurrentListHead=(DB_APP_NODE*)NULL;
static DB_APP_NODE* DbCurrentListTail=(DB_APP_NODE*)NULL;

static int yyerror();
static void sub_pvname(char*,char*);

#ifdef vxWorks
static DBENTRY* pdbentry;
extern struct dbBase *pdbbase;
#endif

%}

%start database

%token <Str> COMMA
%token <Str> WORD VALUE 
%token <Str> FIELD
%left O_BRACE C_BRACE O_PAREN C_PAREN
%left DATABASE RECORD
%left NOWHERE
%token APPL

%union
{
    int	Int;
	char Char;
	char *Str;
	double Real;
}

%%

database:	DATABASE d_head d_body
	| DATABASE d_head /* jbk added for graphical thing */
	| db_components
	;

d_head:	O_PAREN WORD C_PAREN
	{ free($2); }
	| O_PAREN WORD COMMA VALUE C_PAREN
	{ free($2); free($4); }
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
	| db_components applic
	| db_components record
	;

applic: APPL O_PAREN VALUE C_PAREN
	{
		DB_APP_NODE* an=(DB_APP_NODE*)malloc(sizeof(DB_APP_NODE*));

		if(subst_used)
		{
			strcpy(subst_buffer,$<Str>3);
			if(dbDoSubst(subst_buffer,sizeof(subst_buffer),NULL)!=0)
				fprintf(stderr,"dbDoSubst failed\n");
#ifdef vxWorks
			an->name=strdup(subst_buffer);
			free($3);
#else
			printf("\napplication(\"%s\")\n",subst_buffer);
#endif
		}
		else
		{
#ifdef vxWorks
			an->name=$<Str>3;
#else
			printf("\napplication(\"%s\")\n",$<Str>3);
#endif
		}
		if(DbCurrentListHead==(DB_APP_NODE*)NULL) DbCurrentListTail=an;

		an->next=DbCurrentListHead;
		DbCurrentListHead=an;
	}
	;

records: /* null */
	| records record
	;

record:	RECORD r_head r_body
	{
#ifndef vxWorks
		 printf("}\n");
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

#ifdef vxWorks
static char* strdup(char* x)
{
	char* c;
	c=(char*)malloc(strlen(x)+1);
	strcpy(c,x);
	return c;
}
#endif
 
static int is_not_inited = 1;
 
int dbLoadRecords(char* pfilename, char* pattern)
{
	FILE* fp;
	long status;
	DB_APP_NODE* an;

#ifdef vxWorks
	if(pdbbase==NULL)
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

	if(is_not_inited)
	{
		yyin=fp;
		is_not_inited=0;
	}
	else
	{
		yyrestart(fp);
	}

#ifdef vxWorks
	pdbentry=dbAllocEntry(pdbbase);
#endif

	yyparse();

#ifdef vxWorks
	dbFreeEntry(pdbentry);
#endif

	if(subst_used) dbFreeSubst();

	fclose(fp);

	if(DbCurrentListHead==(DB_APP_NODE*)NULL)
	{
		/* set up a default list to put on the master application list */
		DbCurrentListHead=(DB_APP_NODE*)malloc(sizeof(DB_APP_NODE));
		DbCurrentListTail=DbCurrentListHead;
		DbCurrentListHead->name=strdup(pfilename);
		DbCurrentListHead->next=(DB_APP_NODE*)NULL;
	}

	DbCurrentListTail->next=DbApplList;
	DbApplList=DbCurrentListHead;
	DbCurrentListHead=(DB_APP_NODE*)NULL;
	DbCurrentListTail=(DB_APP_NODE*)NULL;

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
			printf("record(%s,\"%s\") {",type,subst_buffer);
#endif
		}
		else
		{
#ifdef vxWorks
			if( dbCreateRecord(pdbentry,name) )
				fprintf(stderr,"Cannot create record %s\n",name);
#else
			printf("record(%s,\"%s\") {",type,name);
#endif
		}
}

#ifdef vxWorks
int dbAppList()
{
	DB_APP_NODE* an;

	for(an=DbApplList;an;an=an->next)
		printf("%s\n",an->name);

	return 0;
}
#endif
