%{
static int yyerror();
static int yy_start;
static int myParse();
#include "asLibRoutines.c"
static int yyFailed = FALSE;
static int line_num=1;
static UAG *yyUag=NULL;
static HAG *yyHag=NULL;
static ASG *yyAsg=NULL;
static ASGRULE *yyAsgRule=NULL;
%}

%start asconfig

%token tokenUAG tokenHAG tokenASG tokenRULE tokenCALC 
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
	|	tokenHAG hag_head hag_body
	|	tokenHAG hag_head
	|	tokenASG asg_head asg_body
	|	tokenASG asg_head
	;

uag_head:	'(' tokenNAME ')'
	{
		yyUag = asUagAdd($2);
		if(!yyUag) yyerror("");
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
			yyerror($1);
		}
		free((void *)$1);
	}
	;

hag_head:	'(' tokenNAME ')'
	{
		yyHag = asHagAdd($2);
		if(!yyHag) yyerror("");
		free((void *)$2);
	}
	;

hag_body:	'{' hag_user_list '}'
	;

hag_user_list:	hag_user_list ',' hag_user_list_name
	|	hag_user_list_name
	;

hag_user_list_name:	tokenNAME
	{
		long	status;

		status = asHagAddHost(yyHag,$1);
		if(status) {
			yyerror("");
		}
		free((void *)$1);
	}
	;

asg_head:	'(' tokenNAME ')'
	{
		yyAsg = asAsgAdd($2);
		if(!yyAsg) yyerror("");
		free((void *)$2);
	}
	;

asg_body:	'{' asg_body_list '}'
	{
	}

asg_body_list:	asg_body_list asg_body_item
	|	asg_body_item

asg_body_item:	inp_config | rule_config 
	;

inp_config:	tokenINP '(' inp_body ')'
	{
		long	status;

		status = asAsgAddInp(yyAsg,$<Str>3,$<Int>1);
		if(status) {
			yyerror("");
		}
		free((void *)$<Str>3);
	}
	;

inp_body:	tokenNAME | tokenPVNAME
	;

rule_config:	tokenRULE rule_head rule_body
	|	tokenRULE rule_head

rule_head:	'(' tokenINTEGER ',' tokenNAME ')'
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
		yyAsgRule = asAsgAddRule(yyAsg,rights,$2);
		free((void *)$4);
	}
	;


rule_body:	'{' rule_list '}'
	;

rule_list:	rule_list rule_list_item
	|	rule_list_item
	;

rule_list_item: tokenUAG '(' rule_uag_list ')'
	|	tokenHAG  '(' rule_hag_list ')'
	|	tokenCALC '(' tokenSTRING ')'
	{
		long status;

		status = asAsgRuleCalc(yyAsgRule,$3);
		if(status){
		    yyerror("access security CALC failure");
		}
		free((void *)$3);
	}
	;

rule_uag_list:	rule_uag_list ',' rule_uag_list_name
	|	rule_uag_list_name
	;

rule_uag_list_name:	tokenNAME
	{
		long status;

		status = asAsgRuleUagAdd(yyAsgRule,$1);
		if(status) {
		    yyerror("");
		}
		free((void *)$1);
	}
	;

rule_hag_list:	rule_hag_list ',' rule_hag_list_name
	|	rule_hag_list_name
	;

rule_hag_list_name:	tokenNAME
	{
		long status;

		status = asAsgRuleHagAdd(yyAsgRule,$1);
		if(status) {
		    yyerror("");
		}
		free((void *)$1);
	}
	;
%%
 
#include "asLib_lex.c"
 
static int yyerror(str)
char  *str;
{
    epicsPrintf("Access Security Error(%s) line %d token %s\n",
	str,line_num,yytext);
    yyFailed = TRUE;
    return(0);
}
static int myParse(ASINPUTFUNCPTR inputfunction)
{
    static int	FirstFlag = 1;
    int		rtnval;
 
    my_yyinput = &inputfunction;
    if (!FirstFlag) {
	line_num=1;
	yyFailed = FALSE;
	yyreset(NULL);
	yyrestart(NULL);
    }
    FirstFlag = 0;
    rtnval = yyparse();
    if(rtnval!=0 || yyFailed) return(-1); else return(0);
}
