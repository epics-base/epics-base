%{
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <epicsVersion.h>

#undef PUKE_FACE
#if EPICS_VERSION<4
#if EPICS_REVISION<13
#if EPICS_MODIFICATION<1 && EPICS_UPDATE_LEVEL<12
#define PUKE_FACE
#endif
#endif
#endif

/* kludge for buggy sun lex/yacc.  exploits the fact that we know the union */
/* below will be given the name YYSTYPE.  done so that ifndef YYSTYPE  */
/* (as it appears in pdb.c) fails */
/* #define YYSTYPE OUR_YYSTYPE */

int line_num;
char Field_val[1000];

%}

%start sf2db

%token <Str> WORD
%token <Str> FIELD
%token <Str> TYPE
%token <Str> PV CLOSE

%union
{
    int	Int;
    char Char;
    char *Str;
    double Real;
}

%%

sf2db:	crap_head records closer
	{ printf("\n"); }
	;

closer:	
	| CLOSE
	;

crap_head: 
	| crap_header CLOSE
	| CLOSE
	| crap_header
	;

crap_header: crap_header WORD
	| WORD
	;

records: records record
	| record
	;

record:	header fields CLOSE
	{ printf("\t}\n"); }
	;

header: PV WORD TYPE WORD WORD
	{ printf("\trecord(%s,\"%s\") {\n",$4,$2); }
	| PV WORD TYPE WORD
	{ printf("\trecord(%s,\"%s\") {\n",$4,$2); }

fields: field
	| fields field
	;

field:	a_field words
	{
		printf("\t\tfield(%s,\"%s\")\n",$<Str>1,Field_val);
	}
	;

a_field: FIELD
	{
		Field_val[0]='\0';
		$<Str>$ = $1;
	}
	;

words: 
	| words a_word
	;

a_word: WORD
	{
		char *p;

		if((p=strstr($1,".PP.MS"))) {p[0]=' ';p[3]=' '; }
		else if((p=strstr($1,".PP.NMS"))) {p[0]=' ';p[3]=' ';}
		else if((p=strstr($1,".NPP.MS"))) {p[0]=' ';p[4]=' ';}
		else if((p=strstr($1,".NPP.NMS"))) {p[0]=' ';p[4]=' ';}
		else { if(strlen(Field_val)>0) strcat(Field_val," "); }

		strcat(Field_val,$1);
	}
	;

%%
 
#include "sf2dbLex.c"
 
yyerror(str)
char  *str;
{ fprintf(stderr,"Error line %d : %s\n",line_num, yytext); }
 
/*-----------------------main routine-----------------------*/
main(argc, argv)
int argc;
char **argv;
{
FILE *fdin;

	/* remember that the database name is not retrieved from the .report
	   file, it is specified as a command line argument */

	if( argc < 2 )
	{
		printf("Usage: %s database_name < dct_report_file > new_gdct_db_file.db\n",
			argv[0]);
		printf("\n\twhere\n\tdatabase_name: the name you wish to give the .database file\n");
		printf("\tdct_report_file: the old dct short form report file\n");
		printf("\tnew_gdct_db_file.db: the new gdct db file name, this file is used as \n\t\tinput to the gdct.\n");
		exit(0);
	}

#ifdef PUKE_HEAD
	printf("database(x) { nowhere() {\n");
#endif
	/* yyreset(); */
	yyparse();
#ifdef PUKE_HEAD
	printf("}}\n");
#endif


/*	fdin = freopen("tester.report", "r+", stdin);
	yyreset();
	yyparse(); */
}

