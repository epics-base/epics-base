%{
static int yyerror();
static int yy_start;
static long pvt_yy_parse(void);
static int yyFailed = 0;
#include "dbAsciiRoutines.c"
static char *menuString = "menu";
%}

%start database

%token tokenINCLUDE tokenPATH
%token tokenMENU tokenCHOICE tokenRECORDTYPE tokenFIELD
%token tokenDEVICE tokenDRIVER tokenBREAKTABLE
%token tokenRECORD
%token <Str> tokenSTRING

%union
{
    char	*Str;
}

%%

database:	database database_item | database_item;

database_item:	include
	|	path
	|	tokenMENU menu_head menu_body
	|	tokenRECORDTYPE recordtype_head recordtype_body
	|	device
	|	driver
	|	tokenBREAKTABLE	break_head break_body
	|	tokenRECORD record_head record_body
	;

include:	tokenINCLUDE tokenSTRING
{
	if(dbDebug>2) printf("include : %s\n",$2);
	dbAsciiIncludeNew($2);
};

path:	tokenPATH tokenSTRING
{
	if(dbDebug>2) printf("path : %s\n",$2);
	dbAsciiPath($2);
};

menu_head:	'(' tokenSTRING ')'
{
	if(dbDebug>2) printf("menu_head %s\n",$2);
	dbAsciiMenuHead($2);
};

menu_body:	'{' choice_list '}'
{
	if(dbDebug>2) printf("menu_body\n");
	dbAsciiMenuBody();
}
	| include ;

choice_list:	choice_list choice | choice;

choice:	tokenCHOICE '(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbDebug>2) printf("choice %s %s\n",$3,$5);
	dbAsciiMenuChoice($3,$5);
} ;

recordtype_head: '(' tokenSTRING ')'
{
	if(dbDebug>2) printf("recordtype_head %s\n",$2);
	dbAsciiRecordtypeHead($2);
};

recordtype_body: '{' recordtype_field_list '}'
{
	if(dbDebug>2) printf("recordtype_body\n");
	dbAsciiRecordtypeBody();
};

recordtype_field_list:	recordtype_field_list recordtype_field
	|	recordtype_field;

recordtype_field: tokenFIELD recordtype_field_head recordtype_field_body
	| include ;

recordtype_field_head:	'(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbDebug>2) printf("recordtype_field_head %s %s\n",$2,$4);
	dbAsciiRecordtypeFieldHead($2,$4);
};

recordtype_field_body:	'{' recordtype_field_item_list '}' ;

recordtype_field_item_list:  recordtype_field_item_list recordtype_field_item
	| recordtype_field_item;

recordtype_field_item:	tokenSTRING '(' tokenSTRING ')' 
{
	if(dbDebug>2) printf("recordtype_field_item %s %s\n",$1,$3);
	dbAsciiRecordtypeFieldItem($1,$3);
}
	| tokenMENU '(' tokenSTRING ')' 
{
	char	*pmenu;

	if(dbDebug>2) printf("recordtype_field_item %s (%s)\n",menuString,$3);
	pmenu = (char *)malloc(strlen(menuString)+1);
	strcpy(pmenu,menuString);
	dbAsciiRecordtypeFieldItem(pmenu,$3);
};


device: tokenDEVICE '('
	tokenSTRING ',' tokenSTRING ',' tokenSTRING ',' tokenSTRING ')'
{ 
	if(dbDebug>2) printf("device %s %s %s %s\n",$3,$5,$7,$9);
	dbAsciiDevice($3,$5,$7,$9);
};


driver: tokenDRIVER '(' tokenSTRING ')'
{
	if(dbDebug>2) printf("driver %s\n",$3);
	dbAsciiDriver($3);
};

break_head: '(' tokenSTRING ')'
{
	if(dbDebug>2) printf("break_head %s\n",$2);
	dbAsciiBreakHead($2);
};

break_body : '{' break_list '}'
{
	if(dbDebug>2) printf("break_body\n");
	dbAsciiBreakBody();
};

break_list: break_list ',' break_item | break_item;

break_item: tokenSTRING
{
	if(dbDebug>2) printf("break_item tokenSTRING %s\n",$1);
	dbAsciiBreakItem($1);
};


record_head: '(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbDebug>2) printf("record_head %s %s\n",$2,$4);
	dbAsciiRecordHead($2,$4);
};

record_body: '{' record_field_list '}'
{
	if(dbDebug>2) printf("record_body\n");
	dbAsciiRecordBody();
};

record_field_list:	record_field_list record_field
	|	record_field;

record_field: tokenFIELD '(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbDebug>2) printf("record_field %s %s\n",$3,$5);
	dbAsciiRecordField($3,$5);
}
	| include ;

%%
 
#include "dbAsciiLex.c"


static int yyerror(char *str)
{
    fprintf(stderr,"Error ");
    if(str) fprintf(stderr,"\"%s\"",str);
    fprintf(stderr,"  Last token \"%s\"\n",yytext);
    dbAsciiIncludePrint(stderr);
    yyFailed = TRUE;
    return(0);
}
static long pvt_yy_parse(void)
{
    static int	FirstFlag = 1;
    long	rtnval;
 
    if (!FirstFlag) {
	yyFailed = FALSE;
	yyreset();
	yyrestart(NULL);
    }
    FirstFlag = 0;
    rtnval = yyparse();
    if(rtnval!=0 || yyFailed) return(-1); else return(0);
}
