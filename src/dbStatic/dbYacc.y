%{
static int yyerror();
static int yy_start;
static long pvt_yy_parse(void);
static int yyFailed = 0;
static int yyAbort = 0;
#include "dbLexRoutines.c"
static char *menuString = "menu";
%}

%start database

%token tokenINCLUDE tokenPATH tokenADDPATH
%token tokenMENU tokenCHOICE tokenRECORDTYPE tokenFIELD
%token tokenDEVICE tokenDRIVER tokenBREAKTABLE
%token tokenRECORD tokenGRECORD
%token <Str> tokenSTRING

%union
{
    char	*Str;
}

%%

database:	database database_item | database_item;

database_item:	include
	|	path
	|	addpath
	|	tokenMENU menu_head menu_body
	|	tokenRECORDTYPE recordtype_head recordtype_body
	|	device
	|	driver
	|	tokenBREAKTABLE	break_head break_body
	|	tokenRECORD record_head record_body
	|	tokenGRECORD grecord_head record_body
	;

include:	tokenINCLUDE tokenSTRING
{
	if(dbStaticDebug>2) printf("include : %s\n",$2);
	dbIncludeNew($2);
};

path:	tokenPATH tokenSTRING
{
	if(dbStaticDebug>2) printf("path : %s\n",$2);
	dbPathCmd($2);
};

addpath:	tokenADDPATH tokenSTRING
{
	if(dbStaticDebug>2) printf("addpath : %s\n",$2);
	dbAddPathCmd($2);
};

menu_head:	'(' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("menu_head %s\n",$2);
	dbMenuHead($2);
};

menu_body:	'{' choice_list '}'
{
	if(dbStaticDebug>2) printf("menu_body\n");
	dbMenuBody();
};

choice_list:	choice_list choice | choice;

choice:	tokenCHOICE '(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("choice %s %s\n",$3,$5);
	dbMenuChoice($3,$5);
} 
	| include;

recordtype_head: '(' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("recordtype_head %s\n",$2);
	dbRecordtypeHead($2);
};

recordtype_body: '{' recordtype_field_list '}'
{
	if(dbStaticDebug>2) printf("recordtype_body\n");
	dbRecordtypeBody();
};

recordtype_field_list:	recordtype_field_list recordtype_field
	|	recordtype_field;

recordtype_field: tokenFIELD recordtype_field_head recordtype_field_body
	| include ;

recordtype_field_head:	'(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("recordtype_field_head %s %s\n",$2,$4);
	dbRecordtypeFieldHead($2,$4);
};

recordtype_field_body:	'{' recordtype_field_item_list '}' ;

recordtype_field_item_list:  recordtype_field_item_list recordtype_field_item
	| recordtype_field_item;

recordtype_field_item:	tokenSTRING '(' tokenSTRING ')' 
{
	if(dbStaticDebug>2) printf("recordtype_field_item %s %s\n",$1,$3);
	dbRecordtypeFieldItem($1,$3);
}
	| tokenMENU '(' tokenSTRING ')' 
{
	char	*pmenu;

	if(dbStaticDebug>2) printf("recordtype_field_item %s (%s)\n",menuString,$3);
	pmenu = (char *)malloc(strlen(menuString)+1);
	strcpy(pmenu,menuString);
	dbRecordtypeFieldItem(pmenu,$3);
};


device: tokenDEVICE '('
	tokenSTRING ',' tokenSTRING ',' tokenSTRING ',' tokenSTRING ')'
{ 
	if(dbStaticDebug>2) printf("device %s %s %s %s\n",$3,$5,$7,$9);
	dbDevice($3,$5,$7,$9);
};


driver: tokenDRIVER '(' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("driver %s\n",$3);
	dbDriver($3);
};

break_head: '(' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("break_head %s\n",$2);
	dbBreakHead($2);
};

break_body : '{' break_list '}'
{
	if(dbStaticDebug>2) printf("break_body\n");
	dbBreakBody();
};

break_list: break_list ',' break_item
	| break_list break_item
	| break_item;

break_item: tokenSTRING
{
	if(dbStaticDebug>2) printf("break_item tokenSTRING %s\n",$1);
	dbBreakItem($1);
};


grecord_head: '(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("grecord_head %s %s\n",$2,$4);
	dbRecordHead($2,$4,1);
};

record_head: '(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("record_head %s %s\n",$2,$4);
	dbRecordHead($2,$4,0);
};

record_body: /*Null*/
{
	if(dbStaticDebug>2) printf("null record_body\n");
	dbRecordBody();
}
	| '{' '}'
{
	if(dbStaticDebug>2) printf("record_body - no fields\n");
	dbRecordBody();
}
        | '{' record_field_list '}'
{
	if(dbStaticDebug>2) printf("record_body\n");
	dbRecordBody();
};

record_field_list:	record_field_list record_field
	|	record_field;

record_field: tokenFIELD '(' tokenSTRING ',' tokenSTRING ')'
{
	if(dbStaticDebug>2) printf("record_field %s %s\n",$3,$5);
	dbRecordField($3,$5);
}
	| include ;

%%
 
#include "dbLex.c"


static int yyerror(char *str)
{
    fprintf(stderr,"Error ");
    if(str) fprintf(stderr,"\"%s\"",str);
    fprintf(stderr,"  Last token \"%s\"\n",yytext);
    dbIncludePrint(stderr);
    yyFailed = TRUE;
    return(0);
}
static long pvt_yy_parse(void)
{
    static int	FirstFlag = 1;
    long	rtnval;
 
    if (!FirstFlag) {
	yyAbort = FALSE;
	yyFailed = FALSE;
	yyreset();
	yyrestart(NULL);
    }
    FirstFlag = 0;
    rtnval = yyparse();
    if(rtnval!=0 || yyFailed) return(-1); else return(0);
}
