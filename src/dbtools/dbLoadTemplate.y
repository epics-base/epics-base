%{

/*************************************************************************\
* Copyright (c) 2006 UChicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "osiUnistd.h"
#include "macLib.h"
#include "dbAccess.h"
#include "dbmf.h"
#include "epicsVersion.h"
#include "epicsExport.h"
#include "dbLoadTemplate.h"

static int line_num;
static int yyerror(char *str);

static char *sub_collect = NULL;
static char **vars = NULL;
static char *db_file_name = NULL;
static int var_count,sub_count;

/* We allocate MAX_VAR_FACTOR chars in the sub_collect string for each
 * "variable=value," segment, and will accept at most dbTemplateMaxVars
 * template variables.  The user can adjust that variable to increase
 * the number of variables or the length allocated for the buffer.
 */
#define MAX_VAR_FACTOR 50

int dbTemplateMaxVars = 100;
epicsExportAddress(int, dbTemplateMaxVars);

%}

%start template

%token <Str> WORD QUOTE
%token DBFILE
%token PATTERN
%token EQUALS COMMA
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
		if(db_file_name)
			dbLoadRecords(db_file_name,NULL);
		else
			fprintf(stderr,"Error: no db file name given\n");
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
	| DBFILE QUOTE
	{
		var_count=0;
		if(db_file_name) dbmfFree(db_file_name);
		db_file_name = dbmfMalloc(strlen($2)+1);
		strcpy(db_file_name,$2);
		dbmfFree($2);
	}
	;

subst: PATTERN pattern subs
	| PATTERN pattern
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
	| vars COMMA var
	| var
	;

var: WORD
	{
            if (var_count >= dbTemplateMaxVars) {
                fprintf(stderr,
                    "More than dbTemplateMaxVars = %d macro variables used\n",
                    dbTemplateMaxVars);
                yyerror(NULL);
            }
            else {
                vars[var_count] = dbmfMalloc(strlen($1)+1);
                strcpy(vars[var_count],$1);
                var_count++;
                dbmfFree($1);
            }
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
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
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
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
		sub_collect[0]='\0';
		sub_count=0;
	}
	;

vals: vals val
	| vals COMMA val
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
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
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
		if(db_file_name)
			dbLoadRecords(db_file_name,sub_collect);
		else
			fprintf(stderr,"Error: no db file name given\n");
		sub_collect[0]='\0';
		sub_count=0;
	}
	;

sub_pats: sub_pats sub_pat
	| sub_pats COMMA sub_pat
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
    if (str)
	fprintf(stderr, "Substitution file error: %s\n", str);
    else
	fprintf(stderr, "Substitution file error.\n");
    fprintf(stderr, "line %d: '%s'\n", line_num, yytext);
    return 0;
}

static int is_not_inited = 1;

int epicsShareAPI dbLoadTemplate(char* sub_file)
{
	FILE *fp;
	int ind;

	line_num=1;

	if( !sub_file || !*sub_file)
	{
		fprintf(stderr,"must specify variable substitution file\n");
		return -1;
	}

        if (dbTemplateMaxVars < 1)
        {
                fprintf(stderr,"Error: dbTemplateMaxVars = %d, must be +ve\n",
                        dbTemplateMaxVars);
                return -1;
        }

	if( !(fp=fopen(sub_file,"r")) )
	{
		fprintf(stderr,"dbLoadTemplate: error opening sub file %s\n",sub_file);
		return -1;
	}

	vars = malloc(dbTemplateMaxVars * sizeof(char*));
	sub_collect = malloc(dbTemplateMaxVars * MAX_VAR_FACTOR);
	if (!vars || !sub_collect)
	{
		free(vars);
		free(sub_collect);
		fclose(fp);
		fprintf(stderr, "dbLoadTemplate: Out of memory!\n");
		return -1;
	}

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

