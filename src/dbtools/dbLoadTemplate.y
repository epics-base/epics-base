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
#include <stdlib.h>

#include "macLib.h"
#include "dbmf.h"
#include "epicsVersion.h"
#ifdef WIN32
#include "getopt.h"
#endif

static int line_num;
static int yyerror();
int dbLoadTemplate(char* sub_file);

#ifdef SUB_TOOL
static int sub_it();
#else
int dbLoadRecords(char* pfilename, char* pattern);
#endif


#ifdef vxWorks
#define VAR_MAX_VAR_STRING 5000
#define VAR_MAX_VARS 100
#else
#define VAR_MAX_VAR_STRING 50000
#define VAR_MAX_VARS 700
#endif

static char *sub_collect = NULL;
static MAC_HANDLE *macHandle = NULL;
static char** vars = NULL;
static char* db_file_name = NULL;
static int var_count,sub_count;

%}

%start template

%token <Str> WORD QUOTE
%token DBFILE
%token PATTERN
%token EQUALS
%left O_PAREN C_PAREN
%left O_BRACE C_BRACE

%union
{
    int	Int;
    char Char;
    char *Str;
    double Real;
}

%%

template: templs
	| subst
	;

templs: templs templ
	| templ
	;

templ: templ_head O_BRACE subst C_BRACE
	| templ_head
	{
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,NULL);
		else
			fprintf(stderr,"Error: no db file name given\n");
#else
		sub_it();
#endif
	}
	;

templ_head: DBFILE WORD
	{
		var_count=0;
		if(db_file_name) dbmfFree(db_file_name);
		db_file_name = dbmfMalloc(strlen($2)+1);
		strcpy(db_file_name,$2);
		dbmfFree($2);
	}
	;

subst: PATTERN pattern subs
    | var_subs
    ;

pattern: O_BRACE vars C_BRACE
	{ 
#ifdef ERROR_STUFF
		int i;
		for(i=0;i<var_count;i++) fprintf(stderr,"variable=(%s)\n",vars[i]);
		fprintf(stderr,"var_count=%d\n",var_count);
#endif
	}
    ;

vars: vars var
	| var
	;

var: WORD
	{
	    vars[var_count++] = dbmfMalloc(strlen($1)+1);
	    strcpy(vars[var_count],$1);
	    dbmfFree($1);
	}
	;

subs: subs sub
	| sub
	;

sub: WORD O_BRACE vals C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
#else
		sub_it();
#endif

		dbmfFree($1);
		sub_collect[0]='\0';
		sub_count=0;
	}
	| O_BRACE vals C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
#else
		sub_it();
#endif

		sub_collect[0]='\0';
		sub_count=0;
	}
	;

vals: vals val
	| val
	;

val: QUOTE
	{
		if(sub_count<=var_count)
		{
			strcat(sub_collect,vars[sub_count]);
			strcat(sub_collect,"=\"");
			strcat(sub_collect,$1);
			strcat(sub_collect,"\",");
			sub_count++;
		}
		dbmfFree($1);
	}
	| WORD
	{
		if(sub_count<=var_count)
		{
			strcat(sub_collect,vars[sub_count]);
			strcat(sub_collect,"=");
			strcat(sub_collect,$1);
			strcat(sub_collect,",");
			sub_count++;
		}
		dbmfFree($1);
	}
	;

var_subs: var_subs var_sub
	| var_sub
	;

var_sub: WORD O_BRACE sub_pats C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
#else
		sub_it();
#endif

		dbmfFree($1);
		sub_collect[0]='\0';
		sub_count=0;
	}
	| O_BRACE sub_pats C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
#ifdef ERROR_STUFF
		fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect);
#endif
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
#else
		sub_it();
#endif

		sub_collect[0]='\0';
		sub_count=0;
	}
	;

sub_pats: sub_pats sub_pat
	| sub_pat
	;

sub_pat: WORD EQUALS WORD
	{
		strcat(sub_collect,$1);
		strcat(sub_collect,"=");
		strcat(sub_collect,$3);
		strcat(sub_collect,",");
		dbmfFree($1); dbmfFree($3);
		sub_count++;
	}
	| WORD EQUALS QUOTE
	{
		strcat(sub_collect,$1);
		strcat(sub_collect,"=\"");
		strcat(sub_collect,$3);
		strcat(sub_collect,"\",");
		dbmfFree($1); dbmfFree($3);
		sub_count++;
	}
	;

%%
 
#include "dbLoadTemplate_lex.c"
 
static int yyerror(char* str)
{
	fprintf(stderr,"Substitution file parse error\n");
	fprintf(stderr,"line %d:%s\n",line_num,yytext);
	return(0);
}

static int is_not_inited = 1;
 
int dbLoadTemplate(char* sub_file)
{
	FILE *fp;
	int ind;

	line_num=0;

	if( !sub_file || !*sub_file)
	{
		fprintf(stderr,"must specify variable substitution file\n");
		return -1;
	}

	if( !(fp=fopen(sub_file,"r")) )
	{
		fprintf(stderr,"dbLoadTemplate: error opening sub file %s\n",sub_file);
		return -1;
	}

	vars = (char**)malloc(VAR_MAX_VARS * sizeof(char*));
	sub_collect = malloc(VAR_MAX_VAR_STRING);
	sub_collect[0]='\0';
	var_count=0;
	sub_count=0;

	if(is_not_inited)
	{
		yyin=fp;
		is_not_inited=0;
	}
	else
	{
		yyrestart(fp);
	}

	yyparse();
	for(ind=0;ind<var_count;ind++) dbmfFree(vars[ind]);
	free(vars);
	free(sub_collect);
	vars = NULL;
	fclose(fp);
	if(db_file_name){
	    dbmfFree((void *)db_file_name);
	    db_file_name = NULL;
	}
	return 0;
}

#ifndef vxWorks
#ifdef SUB_TOOL
/* this is generic substitution on any file */

char* dbfile;

main(int argc, char** argv)
{
	if(argc!=3)
	{
		fprintf(stderr,"Usage: %s file sub_file\n",argv[0]);
		fprintf(stderr,"\n\twhere file is any ascii text file\n");
		fprintf(stderr,"\tsub_file in the variable substitution file\n");
		fprintf(stderr,"\n\tThis program uses the sub_file to perform\n");
		fprintf(stderr,"\tsubstitutions on to standard out.\n");
		exit(1);
	}
	dbfile = argv[1];
	dbLoadTemplate(argv[2]);
	return 0;
}

/* use sub_collect and db_file_name to do work */
static int sub_it()
{
	FILE* fp;
	char var_buff[500];
	char    **macPairs;

#ifdef ERROR_STUFF
	fprintf(stderr,"In sub_it()\n");
#endif

	if( *sub_collect )
	{
#ifdef ERROR_STUFF
	fprintf(stderr," macCreateHandle() calling\n");
#endif
		if(macCreateHandle(&macHandle,NULL)) {
		    fprintf(stderr,"dbLoadTemplate macCreateHandle error\n");
		    exit(1);
		}
		macParseDefns(macHandle,sub_collect,&macPairs);
		if(macPairs == NULL) {
		    macDeleteHandle(macHandle);
		    macHandle = NULL;
		} else {
		    macInstallMacros(macHandle,macPairs);
		    free((void *)macPairs);
		}
	}
	else
	{
		fprintf(stderr,"No valid substitutions found in table\n");
		exit(1);
	}

	if( !(fp=fopen(dbfile,"r")) )
	{
		fprintf(stderr,"sub_tool: error opening file\n");
		exit(1);
	}

	/* do the work here */
	while( fgets(var_buff,200,fp)!=(char*)NULL )
	{
		int n;
#ifdef ERROR_STUFF
		fprintf(stderr," calling macExpandString()\n");
#endif
                n = macExpandString(macHandle,var_buff,sub_collect,
		     VAR_MAX_VAR_STRING-1);
		if(n<0) fprintf(stderr,"macExpandString failed\n"); 
		fputs(sub_collect,stdout);
	}

#ifdef ERROR_STUFF
	fprintf(stderr," calling macDeleteHandle()\n");
#endif
	macDeleteHandle(macHandle);
	macHandle = NULL;
	fclose(fp);
	return 0;
}

#else
/* this is template loader similar to vxWorks one for .db files */
main(int argc, char** argv)
{
	extern char* optarg;
	extern int optind;
	char* name = (char*)NULL;
	int no_error = 1;
	int c;

	while(no_error && (c=getopt(argc,argv,"s:"))!=-1)
	{
		switch(c)
		{
		case 's':
			if(name) dbmfFree(name);
			name = dbmfMalloc(strlen(optarg));
			strcpy(name,optarg);
			break;
		default: no_error=0; break;
		}
	}

	if(!no_error || optind>=argc)
	{
		fprintf(stderr,"Usage: %s <-s name> sub_file\n",argv[0]);
		fprintf(stderr,"\n\twhere name is the output database name and\n");
		fprintf(stderr,"\tsub_file in the variable substitution file\n");
		fprintf(stderr,"\n\tThis program used the sub_file to produce a\n");
		fprintf(stderr,"\tdatabase of name name to standard out.\n");
		exit(1);
	}

	if(!name) {
	    name = dbmfMalloc(strlen("Composite") + 1);
	    strcpy(name,"Composite");
	}
	dbLoadTemplate(argv[1]);
	dbmfFree((void *)name);
}
#endif
#endif
