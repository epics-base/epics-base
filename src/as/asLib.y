%{
static int yyerror();
static int yy_start;
static int myParse();
#include "asLibRoutines.c"
static int line_num=1;
static UAG *yyUag=NULL;
static LAG *yyLag=NULL;
static ASG *yyAsg=NULL;
static ASGLEVEL *yyAsgLevel=NULL;
%}

%start asconfig

%token tokenUAG tokenLAG tokenASG tokenLEVEL tokenCALC 
%token <Str> tokenINP
%token <Int> tokenINTEGER
%token <Str> tokenNAME tokenPVNAME tokenSTRING

%union
{
    int	Int;
    char Char;
    char *Str;
    double Real;
}

%%

asconfig:	asconfig asconfig_item
	|	asconfig_item

asconfig_item:	tokenUAG uag_head uag_body
	|	tokenUAG uag_head
	|	tokenLAG lag_head lag_body
	|	tokenLAG lag_head
	|	tokenASG asg_head asg_body
	|	tokenASG asg_head
	;

uag_head:	'(' tokenNAME ')'
	{
		yyUag = asUagAdd($2);
		if(!yyUag) yyerror($2);
		free((void *)$2);
	}
	;

uag_body:	'{' uag_user_list '}'
	{
		 ;
	}
	;

uag_user_list:	uag_user_list ',' uag_user_list_name
	|	uag_user_list_name
	;

uag_user_list_name:	tokenNAME
	{
		long	status;

		status = asUagAddUser(yyUag,$1);
		if(status) {
			errMessage(status,"Error while adding UAG");
			yyerror($1);
		}
		free((void *)$1);
	}
	;

lag_head:	'(' tokenNAME ')'
	{
		yyLag = asLagAdd($2);
		if(!yyLag) yyerror($2);
		free((void *)$2);
	}
	;

lag_body:	'{' lag_user_list '}'
	;

lag_user_list:	lag_user_list ',' lag_user_list_name
	|	lag_user_list_name
	;

lag_user_list_name:	tokenNAME
	{
		long	status;

		status = asLagAddLocation(yyLag,$1);
		if(status) {
			errMessage(status,"Error while adding LAG");
			yyerror($1);
		}
		free((void *)$1);
	}
	;

asg_head:	'(' tokenNAME ')'
	{
		yyAsg = asAsgAdd($2);
		if(!yyAsg) yyerror($2);
		free((void *)$2);
	}
	;

asg_body:	'{' asg_body_list '}'
	{
	}

asg_body_list:	asg_body_list asg_body_item
	|	asg_body_item

asg_body_item:	inp_config | level_config 
	;

inp_config:	tokenINP '(' inp_body ')'
	{
		long	status;

		status = asAsgAddInp(yyAsg,$<Str>3,$<Int>1);
		if(status) {
			errMessage(status,"Error while adding INP");
			yyerror($1);
		}
		free((void *)$<Str>3);
	}
	;

inp_body:	tokenNAME | tokenPVNAME
	;

level_config:	tokenLEVEL level_head level_body
	|	tokenLEVEL level_head

level_head:	'(' tokenINTEGER ',' tokenNAME ')'
	{
		asAccessRights	rights;

		if((strcmp($4,"NONE")==0)) {
			rights=asNOACCESS;
		} else if((strcmp($4,"READ")==0)) {
			rights=asREAD;
		} else if((strcmp($4,"WRITE")==0)) {
			rights=asWRITE;
		} else {
			yyerror("Illegal access type");
			rights = asNOACCESS;
		}
		yyAsgLevel = asAsgAddLevel(yyAsg,rights,$2);
		free((void *)$4);
	}
	;


level_body:	'{' level_list '}'
	;

level_list:	level_list level_list_item
	|	level_list_item
	;

level_list_item: tokenUAG '(' level_uag_list ')'
	|	tokenLAG  '(' level_lag_list ')'
	|	tokenCALC '(' tokenSTRING ')'
	{
		long status;

		status = asAsgLevelCalc(yyAsgLevel,$3);
		if(status){
		    errMessage(status,$3);
		    yyerror("CALC failure");
		}
		free((void *)$3);
	}
	;

level_uag_list:	level_uag_list ',' level_uag_list_name
	|	level_uag_list_name
	;

level_uag_list_name:	tokenNAME
	{
		long status;

		status = asAsgLevelUagAdd(yyAsgLevel,$1);
		if(status) {
		    errMessage(status,"Error while adding UAG");
		    yyerror($1);
		}
		free((void *)$1);
	}
	;

level_lag_list:	level_lag_list ',' level_lag_list_name
	|	level_lag_list_name
	;

level_lag_list_name:	tokenNAME
	{
		long status;

		status = asAsgLevelLagAdd(yyAsgLevel,$1);
		if(status) {
		    errMessage(status,"Error while adding LAG");
		    yyerror($1);
		}
		free((void *)$1);
	}
	;
%%
 
#include "asLib_lex.c"
 
static int yyerror(str)
char  *str;
{
    fprintf(stderr,"Error line %d : %s %s\n",line_num,str,yytext);
    return(0);
}
static int myParse(ASINPUTFUNCPTR inputfunction)
{
    static	int FirstFlag = 1;
 
    my_yyinput = &inputfunction;
    if (!FirstFlag) {
	line_num=1;
	yyreset(NULL);
	yyrestart(NULL);
    }
    FirstFlag = 0;
    return(yyparse());
}
