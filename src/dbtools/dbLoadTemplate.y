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
#include <stdlib.h>
#else
#include <memory.h>
#include <malloc.h>
#endif

#include "dbVarSub.h"
#include <epicsVersion.h>

static int line_num;
static int yyerror();
int dbLoadTemplate(char* sub_file);

#ifdef SUB_TOOL
static int sub_it();
#else
int dbLoadRecords(char* pfilename, char* pattern, char* container);
#endif

static char sub_collect[VAR_MAX_VAR_STRING];
static char** vars;
static char* db_file_name = (char*)NULL;
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
	;

templ_head: DBFILE WORD
	{
		var_count=0;
		if(db_file_name) free(db_file_name);
		db_file_name=$2;
	}
	;

subst: PATTERN pattern subs
    | var_subs
    ;

pattern: O_BRACE vars C_BRACE
	{ 
	/*
		int i;
		for(i=0;i<var_count;i++) fprintf(stderr,"variable=(%s)\n",vars[i]);
		fprintf(stderr,"var_count=%d\n",var_count);
	*/
	}
    ;

vars: vars var
	| var
	;

var: WORD
	{
		vars[var_count++]=$1;
	}
	;

subs: subs sub
	| sub
	;

sub: WORD O_BRACE vals C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
		/* fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect); */
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect,$1);
		else
			fprintf(stderr,"Error: no db file name given\n");
#else
		sub_it();
#endif

		free($1);
		sub_collect[0]='\0';
		sub_count=0;
	}
	| O_BRACE vals C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
		/* fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect); */
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect,NULL);
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
			free($1);
			sub_count++;
		}
	}
	| WORD
	{
		if(sub_count<=var_count)
		{
			strcat(sub_collect,vars[sub_count]);
			strcat(sub_collect,"=");
			strcat(sub_collect,$1);
			strcat(sub_collect,",");
			free($1);
			sub_count++;
		}
	}
	;

var_subs: var_subs var_sub
	| var_sub
	;

var_sub: WORD O_BRACE sub_pats C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
		/* fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect); */
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect,$1);
		else
			fprintf(stderr,"Error: no db file name given\n");
#else
		sub_it();
#endif

		free($1);
		sub_collect[0]='\0';
		sub_count=0;
	}
	| O_BRACE sub_pats C_BRACE
	{
		sub_collect[strlen(sub_collect)-1]='\0';
		/* fprintf(stderr,"dbLoadRecords(%s)\n",sub_collect); */
#ifndef SUB_TOOL
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect,NULL);
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
		free($1); free($3);
		sub_count++;
	}
	| WORD EQUALS QUOTE
	{
		strcat(sub_collect,$1);
		strcat(sub_collect,"=\"");
		strcat(sub_collect,$3);
		strcat(sub_collect,"\",");
		free($1); free($3);
		sub_count++;
	}
	;

%%
 
#include "dbLoadTemplate_lex.c"
 
static int yyerror(str)
char  *str;
{ fprintf(stderr,"templ file parse, Error line %d : %s\n",line_num, yytext); }

static int is_not_inited = 1;
 
int dbLoadTemplate(char* sub_file)
{
	FILE *fp;
	long status;

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
	sub_collect[0]='\0';
	var_count=0;
	sub_count=0;

	if(is_not_inited)
	{
		yyin=fp;
		is_not_inited=0;
	}
	else yyrestart(fp);

	yyparse();

	free(vars);
	fclose(fp);
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
int sub_it()
{
	FILE* fp;
	char var_buff[500];
	if( *sub_collect )
		dbInitSubst(sub_collect);
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
		dbDoSubst(var_buff,500,NULL);
		fputs(var_buff,stdout);
	}

	dbFreeSubst();
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
			if(name) free(name);
			name = strdup(optarg);
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

	if(!name) name = "Composite";

	printf("database(name,\"%d.%d\") {\n",EPICS_VERSION,EPICS_REVISION,name);
	dbLoadTemplate(argv[1]);
	printf("}\n");
}
#endif
#endif
