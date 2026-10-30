/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
%{
static int yyerror(char *);
static int yy_start;
#include "asLibRoutines.c"
static int yyFailed = FALSE;
static int yyWarned = FALSE;
static int line_num=1;
static UAG *yyUag=NULL;
static HAG *yyHag=NULL;
static ASG *yyAsg=NULL;
static ASGRULE *yyAsgRule=NULL;

static
char* yystrdup(const char *inp) {
    char* ret = strdup(inp);
    if(!ret)
        yyerror("MALLOC");
    return ret;
}

%}

%start asconfig

%token tokenUAG tokenHAG tokenASG tokenRULE tokenCALC
%token <Str> tokenSTRING
%token <Int64> tokenINT64 tokenINP
%token <Float64> tokenFLOAT64

%union
{
    epicsInt64 Int64;
    epicsFloat64 Float64;
    char *Str;
}

%type <Str> non_rule_keyword
%type <Str> generic_block_elem_name
%type <Str> generic_block_elem
%type <Str> rule_generic_block_elem
%type <Str> rule_generic_block_elem_name
%type <Str> keyword

%%

asconfig:   asconfig asconfig_item
    |   asconfig_item

asconfig_item:  tokenUAG uag_head uag_body
    |   tokenUAG uag_head
    |   tokenHAG hag_head hag_body
    |   tokenHAG hag_head
    |   tokenASG asg_head asg_body
    |   tokenASG asg_head
    |   generic_item
    ;

/* uniformally yield a string for use in warning messages */
keyword: tokenUAG
    { $$ = yystrdup("UAG"); }
    | tokenHAG
    { $$ = yystrdup("HAG"); }
    | tokenCALC
    { $$ = yystrdup("CALC"); }
    | non_rule_keyword
    ;

non_rule_keyword: tokenASG
    { $$ = yystrdup("ASG"); }
    | tokenRULE
    { $$ = yystrdup("RULE"); }
    | tokenINP
    {
        if(!!($$ = yystrdup("INPA")))
            $$[3] += $1; /* 'A' + input number */
    }
    ;

generic_item: tokenSTRING generic_head generic_list_block
    {
        yywarn("Ignoring unsupported TOP LEVEL nested block", $1);
        free($1);
    }
    |   tokenSTRING generic_head generic_block
    {
        yywarn("Ignoring unsupported TOP LEVEL block", $1);
        free($1);
    }
    |   tokenSTRING generic_head
    {
        yywarn("Ignoring unsupported TOP LEVEL bare block", $1);
        free($1);
    }
    ;

generic_head:   '(' ')'
    | '(' generic_element ')'
    | '(' generic_list ')'
    ;

generic_list_block:   '{' generic_element '}'
    '{' generic_list '}'
    ;

generic_list:  generic_list ',' generic_element
    |   generic_element ',' generic_element
    ;

generic_element:  keyword
    |   tokenSTRING
    {
        free($1);
    }
    |   tokenINT64
    |   tokenFLOAT64
    ;

generic_block:   '{' generic_element '}'
    |   '{' generic_list '}'
    |   '{' generic_block_list '}'
    ;

generic_block_list:  generic_block_list generic_block_elem
    {
        free($2);
    }
    |   generic_block_elem
    {
        free($1);
    }
    ;

generic_block_elem: generic_block_elem_name generic_head generic_block
    {
        $$ = $1;
    }
    |   generic_block_elem_name generic_head
    {
        $$ = $1;
    }
    ;

generic_block_elem_name:  keyword
    |   tokenSTRING
    ;

rule_generic_block_elem: rule_generic_block_elem_name generic_head generic_block
    {
        $$ = $1;
    }
    |   rule_generic_block_elem_name generic_head
    {
        $$ = $1;
    }
    ;

rule_generic_block_elem_name:  non_rule_keyword
    |   tokenSTRING
    ;

uag_head:   '(' tokenSTRING ')'
    {
        yyUag = asUagAdd($2);
        if(!yyUag) yyerror("");
        free($2);
    }
    ;

uag_body:   '{' uag_user_list '}'
    {
         ;
    }
    ;

uag_user_list:  uag_user_list ',' uag_user_list_name
    |   uag_user_list_name
    ;

uag_user_list_name: tokenSTRING
    {
        if (asUagAddUser(yyUag,$1))
            yyerror("");
        free($1);
    }
    ;

hag_head:   '(' tokenSTRING ')'
    {
        yyHag = asHagAdd($2);
        if(!yyHag) yyerror("");
        free($2);
    }
    ;

hag_body:   '{' hag_host_list '}'
    ;

hag_host_list:  hag_host_list ',' hag_host_list_name
    |   hag_host_list_name
    ;

hag_host_list_name: tokenSTRING
    {
        if (asHagAddHost(yyHag,$1))
            yyerror("");
        free($1);
    }
    ;

asg_head:   '(' tokenSTRING ')'
    {
        yyAsg = asAsgAdd($2);
        if(!yyAsg) yyerror("");
        free($2);
    }
    ;

asg_body:   '{' asg_body_list '}'
    {
    }

asg_body_list:  asg_body_list asg_body_item
    |   asg_body_item

asg_body_item:  inp_config | rule_config
    ;

inp_config: tokenINP '(' tokenSTRING ')'
    {
        if (asAsgAddInp(yyAsg,$3,(int)$<Int64>1))
            yyerror("");
        free($3);
    }
    ;

rule_config:    tokenRULE rule_head rule_body
    |   tokenRULE rule_head

rule_head: '(' rule_head_mandatory ',' rule_log_option ')'
    | '(' rule_head_mandatory  ')'
    ;


rule_head_mandatory:    tokenINT64 ',' tokenSTRING
    {
        if ($1 < 0) {
            char message[60];
            sprintf(message, "RULE: LEVEL must be positive: %lld", $1);
            yyerror(message);
        } else if((strcmp($3,"NONE")==0)) {
            yyAsgRule = asAsgAddRule(yyAsg,asNOACCESS,(int)$1);
        } else if((strcmp($3,"READ")==0)) {
            yyAsgRule = asAsgAddRule(yyAsg,asREAD,(int)$1);
        } else if((strcmp($3,"WRITE")==0)) {
            yyAsgRule = asAsgAddRule(yyAsg,asWRITE,(int)$1);
        } else {
            yywarn("Ignoring RULE that contains an unsupported keyword", $3);
        }
        free($3);
    }
    ;

rule_log_option:  tokenSTRING
    {
        if((strcmp($1,"TRAPWRITE")==0)) {
            long status;
            status = asAsgAddRuleOptions(yyAsgRule,AS_TRAP_WRITE);
            if(status) yyerror("");
        } else if((strcmp($1,"NOTRAPWRITE")!=0)) {
            yyerror("Log options must be TRAPWRITE or NOTRAPWRITE");
        }
        free($1);
    }
    ;

rule_body:  '{' rule_list '}'
    ;

rule_list:  rule_list rule_list_item
    |   rule_list_item
    ;

rule_list_item: tokenUAG '(' rule_uag_list ')'
    |   tokenHAG  '(' rule_hag_list ')'
    |   tokenCALC '(' tokenSTRING ')'
    {
        if (asAsgRuleCalc(yyAsgRule,$3))
            yyerror("");
        free($3);
    }
    | rule_generic_block_elem
    {
        yywarn("Ignoring RULE that contains an unsupported keyword", $1);
        free($1);
        if (asAsgRuleDisable(yyAsgRule))
            yyerror("");
    }
    ;

rule_uag_list:  rule_uag_list ',' rule_uag_list_name
    |   rule_uag_list_name
    ;

rule_uag_list_name: tokenSTRING
    {
        if (asAsgRuleUagAdd(yyAsgRule,$1))
            yyerror("");
        free($1);
    }
    ;

rule_hag_list:  rule_hag_list ',' rule_hag_list_name
    |   rule_hag_list_name
    ;

rule_hag_list_name: tokenSTRING
    {
        if (asAsgRuleHagAdd(yyAsgRule,$1))
            yyerror("");
        free($1);
    }
    ;
%%

#include "asLib_lex.c"

static int yyerror(char *str)
{
    if (strlen(str))
        fprintf(stderr, ERL_ERROR " %s at line %d\n", str, line_num);
    else
        fprintf(stderr, ERL_ERROR " at line %d\n", line_num);
    yyFailed = TRUE;
    return 0;
}
static int yywarn(char *str, char *token)
{
    if (!yyWarned && strlen(str) && strlen(token))
        fprintf(stderr, ERL_WARNING " %s at line %d: %s\n", str, line_num, token);
    yyWarned = TRUE;
    return 0;
}
static int myParse(ASINPUTFUNCPTR inputfunction)
{
    static int  FirstFlag = 1;
    int         rtnval;

    my_yyinput = &inputfunction;
    if (!FirstFlag) {
        line_num=1;
        yyFailed = FALSE;
        yyWarned = FALSE;
        yyreset();
        yyrestart(NULL);
    }
    FirstFlag = 0;
    rtnval = yyparse();
    if(rtnval!=0 || yyFailed) return(-1); else return(0);
}
