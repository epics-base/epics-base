/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
%{
static int yyerror(char *);
static int yy_start;
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
%token <Str> tokenSTRING

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

uag_head:	'(' tokenSTRING ')'
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

uag_user_list_name:	tokenSTRING
	{
		if (asUagAddUser(yyUag,$1))
			yyerror("");
		free((void *)$1);
	}
	;

hag_head:	'(' tokenSTRING ')'
	{
		yyHag = asHagAdd($2);
		if(!yyHag) yyerror("");
		free((void *)$2);
	}
	;

hag_body:	'{' hag_host_list '}'
	;

hag_host_list:	hag_host_list ',' hag_host_list_name
	|	hag_host_list_name
	;

hag_host_list_name:	tokenSTRING
	{
		if (asHagAddHost(yyHag,$1))
			yyerror("");
		free((void *)$1);
	}
	;

asg_head:	'(' tokenSTRING ')'
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

inp_config:	tokenINP '(' tokenSTRING ')'
	{
		if (asAsgAddInp(yyAsg,$3,$<Int>1))
			yyerror("");
		free((void *)$3);
	}
	;

rule_config:	tokenRULE rule_head rule_body
	|	tokenRULE rule_head

rule_head: rule_head_manditory rule_head_options

rule_head_manditory:	'(' tokenINTEGER ',' tokenSTRING 
	{
		asAccessRights	rights;

		if((strcmp($4,"NONE")==0)) {
			rights=asNOACCESS;
		} else if((strcmp($4,"READ")==0)) {
			rights=asREAD;
		} else if((strcmp($4,"WRITE")==0)) {
			rights=asWRITE;
		} else {
			yyerror("Access rights must be NONE, READ or WRITE");
			rights = asNOACCESS;
		}
		yyAsgRule = asAsgAddRule(yyAsg,rights,$2);
		free((void *)$4);
	}
	;

rule_head_options: ')'
        |          rule_log_options

rule_log_options:  ',' tokenSTRING ')'
        {
                if((strcmp($2,"TRAPWRITE")==0)) {
                        long status;
                        status = asAsgAddRuleOptions(yyAsgRule,AS_TRAP_WRITE);
                        if(status) yyerror("");
                } else if((strcmp($2,"NOTRAPWRITE")!=0)) {
                        yyerror("Log options must be TRAPWRITE or NOTRAPWRITE");
                }
                free((void *)$2);
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
		if (asAsgRuleCalc(yyAsgRule,$3))
			yyerror("");
		free((void *)$3);
	}
	;

rule_uag_list:	rule_uag_list ',' rule_uag_list_name
	|	rule_uag_list_name
	;

rule_uag_list_name:	tokenSTRING
	{
		if (asAsgRuleUagAdd(yyAsgRule,$1))
			yyerror("");
		free((void *)$1);
	}
	;

rule_hag_list:	rule_hag_list ',' rule_hag_list_name
	|	rule_hag_list_name
	;

rule_hag_list_name:	tokenSTRING
	{
		if (asAsgRuleHagAdd(yyAsgRule,$1))
			yyerror("");
		free((void *)$1);
	}
	;
%%
 
#include "asLib_lex.c"
 
static int yyerror(char *str)
{
    if (strlen(str)) epicsPrintf("%s\n", str);
    epicsPrintf("Access Security file error at line %d\n",
	line_num);
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
	yyreset();
	yyrestart(NULL);
    }
    FirstFlag = 0;
    rtnval = yyparse();
    if(rtnval!=0 || yyFailed) return(-1); else return(0);
}
