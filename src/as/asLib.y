%{
static int yyerror();
#include "asLibRoutines.c"
static int line_num=1;
static UAG *yyUag=NULL;
static LAG *yyLag=NULL;
static ASG *yyAsg=NULL;
static ASGLEVEL *yyAsgLevel=NULL;
static int yyLevelLow,yyLevelHigh;
%}

%start asconfig

%token tokenUAG tokenLAG tokenASG tokenLEVEL tokenCALC 
%token <Str> tokenINP
%token <Int> tokenINTEGER
%token <Str> tokenNAME tokenPVNAME tokenSTRING
%left O_BRACE C_BRACE O_PAREN C_PAREN

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
	|	tokenLAG lag_head lag_body
	|	tokenASG asg_head asg_body
	;

uag_head:	O_PAREN tokenNAME C_PAREN
	{
		yyUag = asUagAdd($2);
		if(!yyUag) yyerror($2);
		free((void *)$2);
	}
	;

uag_body:	O_BRACE uag_user_list C_BRACE
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

lag_head:	O_PAREN tokenNAME C_PAREN
	{
		yyLag = asLagAdd($2);
		if(!yyLag) yyerror($2);
		free((void *)$2);
	}
	;

lag_body:	O_BRACE lag_user_list C_BRACE
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

asg_head:	O_PAREN tokenNAME C_PAREN
	{
		yyAsg = asAsgAdd($2);
		if(!yyAsg) yyerror($2);
		free((void *)$2);
	}
	;

asg_body:	O_BRACE asg_body_list C_BRACE
	{
	}

asg_body_list:	asg_body_list asg_body_item
	|	asg_body_item

asg_body_item:	inp_config | level_config 
	;

inp_config:	tokenINP O_PAREN inp_body C_PAREN
	{
		long	status;

		status = asAsgAddInp(yyAsg,$<Str>3,(*(($<Str>1) + 3)-'A'));
		if(status) {
			errMessage(status,"Error while adding INP");
			yyerror($<Str>1);
		}
		free((void *)$<Str>3);
	}
	;

inp_body:	tokenNAME | tokenPVNAME
	;

level_config:	tokenLEVEL level_head level_body
	|	tokenLEVEL level_head

level_head:	O_PAREN level_range ',' tokenNAME C_PAREN
	{
		asAccessRights	rights;

		if((strcmp($4,"READ")==0)) {
			rights=asREAD;
		} else if((strcmp($4,"WRITE")==0)) {
			rights=asWRITE;
		} else {
			yyerror("Illegal access type");
			rights = asNOACCESS;
		}
		yyAsgLevel = asAsgAddLevel(yyAsg,rights,yyLevelLow,yyLevelHigh);
		free((void *)$4);
	}
	;

level_range:	tokenINTEGER
	{
		yyLevelLow = yyLevelHigh = $1;
	}
	|	tokenINTEGER '-' tokenINTEGER
	{
		yyLevelLow = $1;
		yyLevelHigh = $3;
	}
	

level_body:	O_BRACE level_list C_BRACE
	;

level_list:	level_list level_list_item
	|	level_list_item
	;

level_list_item: tokenUAG O_PAREN level_uag_list C_PAREN
	|	tokenLAG  O_PAREN level_lag_list C_PAREN
	|	tokenCALC O_PAREN tokenSTRING C_PAREN
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
		if(asAsgLevelUagAdd(yyAsgLevel,$1))
		    yyerror("Why did asAsgLevelUagAdd fail");
		free((void *)$1);
	}
	;

level_lag_list:	level_lag_list ',' level_lag_list_name
	|	level_lag_list_name
	;

level_lag_list_name:	tokenNAME
	{
		if(asAsgLevelLagAdd(yyAsgLevel,$1))
		    yyerror("Why did asAsgLevelLagAdd fail");
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
	yyrestart(NULL);
  	FirstFlag = 0;
    }
    return(yyparse());
}
