%{
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <dbStaticLib.h>
/* kludge for buggy lex/yacc.  exploits the fact that we know the union */
/* below will be given the name YYSTYPE.  done so that ifndef YYSTYPE  */
/* (as it appears in pdb.c) fails */
#define YYSTYPE OUR_YYSTYPE
int line_num;

extern DBENTRY *pdbentry;
char	rectype[100];
char	recname[100];

char curr_field[10];
char curr_value[200];
char message[200];
long status;

#ifdef __STDC__
static void printMessage(char *mess) {
#else
static void printMessage(mess)
char *mess;
{
#endif /*__STDC__*/

    sprintf(message,"%s Error near line %d Type: %s Name: %s Field: %s Value: %s",
	mess,line_num,rectype,recname,curr_field,curr_value);
    errMessage(status,message);
}
%}

%start conv

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

conv:	things
	;

things:
	| things thing
	;

thing:	header fields CLOSE
	| header CLOSE
	| junko
	;

header: PV WORD TYPE WORD WORD
{
	strcpy(rectype,$4);
	strcpy(recname,$2);
	status = dbFindRecdes(pdbentry,rectype);
	if(status) printMessage("dbFindRecdes");
	status = dbCreateRecord(pdbentry,recname);
	if(status) printMessage("dbCreateRecord");
}

fields: field
	| fields field
	;

field:	f_name words
{
	status = dbPutString(pdbentry,curr_value);
	if(status) printMessage("dbPutString");
}
	;

f_name: FIELD
{

	curr_value[0]='\0';
	strcpy(curr_field,$1);
	status = dbFindField(pdbentry,$1);
	if(status) printMessage("dbFindField");
}
	;

words: 
	| words WORD
{
	char* p;
	/* pretty crappy stuff */
	if((p=strstr($2,".PP.MS"))) {p[0]=' ';p[3]=' ';}
	else if((p=strstr($2,".PP.NMS"))) {p[0]=' ';p[3]=' ';}
	else if((p=strstr($2,".NPP.MS"))) {p[0]=' ';p[4]=' ';}
	else if((p=strstr($2,".NPP.NMS"))) {p[0]=' ';p[4]=' ';}
	else if(strlen(curr_value)>0) { strcat(curr_value," "); }
	strcat(curr_value,$2);
}
	;

junko:	WORD
	| CLOSE
	;

%%
 
#include "atdb_lex.c"
 
yyerror(str)
char  *str;
{
    sprintf(message,"Error line %d : %s\n",line_num, yytext);
    errMessage(-1,message);
}
 
yywrap() { return(1); }
 
