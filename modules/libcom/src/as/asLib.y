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
static int line_num=1;
static UAG *yyUag=NULL;
static HAG *yyHag=NULL;
static ASG *yyAsg=NULL;
static ASGRULE *yyAsgRule=NULL;
static int yyInp=0;
%}

%start asconfig

%token tokenUAG tokenHAG tokenASG tokenRULE tokenCALC
%token tokenMETHOD tokenAUTHORITY
%token tokenREAD tokenWRITE tokenRPC tokenNONE
%token <Int> tokenINP
%token <Int> tokenINTEGER
%token <Str> tokenSTRING

%token tokenYAML_START
%token tokenYAML_USERS tokenYAML_HOSTS tokenYAML_LINKS tokenYAML_RULES
%token tokenYAML_TRAPWRITE tokenYAML_ISTLS tokenYAML_ISNOTTLS
%token <Int> tokenYAML_LEVEL
%token <Str> tokenYAML_NAME
%token <Str> tokenYAML_VERSION
%token <Int> tokenYAML_ACCESS
%token <Str> tokenYAML_CALC
%token <Str> tokenYAML_INP
%type  <Int> access_right

%union
{
    int Int;
    char *Str;
}

%%

asconfig: yaml_config
    |    legacy_config
    ;

yaml_config: tokenYAML_START yaml_content
    ;

legacy_config: legacy_config legacy_item
    |   legacy_item
    ;

legacy_item:  tokenUAG uag_head uag_body
    |   tokenUAG uag_head
    |   tokenHAG hag_head hag_body
    |   tokenHAG hag_head
    |   tokenASG asg_head asg_body
    |   tokenASG asg_head
    ;

uag_head:   '(' tokenSTRING ')'
    {
        yyUag = asUagAdd($2);
        if(!yyUag) yyerror("");
        free((void *)$2);
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
        free((void *)$1);
    }
    ;

hag_head:   '(' tokenSTRING ')'
    {
        yyHag = asHagAdd($2);
        if(!yyHag) yyerror("");
        free((void *)$2);
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
        free((void *)$1);
    }
    ;

asg_head:   '(' tokenSTRING ')'
    {
        yyAsg = asAsgAdd($2);
        if(!yyAsg) yyerror("");
        free((void *)$2);
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
        if (asAsgAddInp(yyAsg,$3,$1))
            yyerror("");
        free((void *)$3);
    }
    ;

rule_config:    tokenRULE rule_head rule_body
    |   tokenRULE rule_head

rule_head: rule_head_manditory rule_head_options


rule_head_manditory:    '(' tokenINTEGER ',' access_right
    {
        yyAsgRule = asAsgAddRule(yyAsg,$4,$2);
    }
    ;

access_right: tokenREAD    { $$ = asREAD; }
    |         tokenWRITE   { $$ = asWRITE; }
    |         tokenRPC     { $$ = asRPC; }
    |         tokenNONE    { $$ = asNOACCESS; }
    ;

rule_head_options: ')'
        |          rule_log_options

rule_log_options:  ',' tokenSTRING ')'
    {
        if((strcmp($2,"TRAPWRITE")==0)) {
            long status;
            status = asAsgAddRuleOptions(yyAsgRule,AS_TRAP_WRITE);
            if(status) yyerror("");
        } else if((strcmp($2,"ISTLS")==0)) {
            long status;
            status = asAsgAddRuleTLSOption(yyAsgRule,1);
            if(status) yyerror("");
        } else if((strcmp($2,"NOTRAPWRITE")!=0)) {
            yyerror("Log options must be TRAPWRITE or NOTRAPWRITE");
        }
        free((void *)$2);
    }
    |   ',' tokenSTRING ',' tokenSTRING ')'
    {
        if((strcmp($2,"TRAPWRITE")==0)) {
            long status;
            status = asAsgAddRuleOptions(yyAsgRule,AS_TRAP_WRITE);
            if(status) yyerror("");
        } else if((strcmp($2,"ISTLS")==0)) {
            long status;
            status = asAsgAddRuleTLSOption(yyAsgRule,1);
            if(status) yyerror("");
        } else if((strcmp($2,"NOTRAPWRITE")!=0)) {
            yyerror("Log options must be TRAPWRITE or NOTRAPWRITE");
        }
        free((void *)$2);
        if((strcmp($4,"TRAPWRITE")==0)) {
            long status;
            status = asAsgAddRuleOptions(yyAsgRule,AS_TRAP_WRITE);
            if(status) yyerror("");
        } else if((strcmp($4,"ISTLS")==0)) {
            long status;
            status = asAsgAddRuleTLSOption(yyAsgRule,1);
            if(status) yyerror("");
        } else if((strcmp($4,"NOTRAPWRITE")!=0)) {
            yyerror("Log options must be TRAPWRITE or NOTRAPWRITE");
        }
        free((void *)$4);
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
        free((void *)$3);
    }
    |   tokenMETHOD '(' rule_method_list ')'
    |   tokenAUTHORITY '(' rule_authority_list ')'
    ;

rule_uag_list:  rule_uag_list ',' rule_uag_list_name
    |   rule_uag_list_name
    ;

rule_uag_list_name: tokenSTRING
    {
        if (asAsgRuleUagAdd(yyAsgRule,$1))
            yyerror("");
        free((void *)$1);
    }
    ;

rule_hag_list:  rule_hag_list ',' rule_hag_list_name
    |   rule_hag_list_name
    ;

rule_hag_list_name: tokenSTRING
    {
        if (asAsgRuleHagAdd(yyAsgRule,$1))
            yyerror("");
        free((void *)$1);
    }
    ;

rule_method_list: rule_method_list ',' rule_method_list_name
    |   rule_method_list_name
    ;

rule_method_list_name: tokenSTRING
    {
        if (asAsgRuleMethodAdd(yyAsgRule, $1))
            yyerror("");
        free((void *)$1);
    }
    ;

rule_authority_list: rule_authority_list ',' rule_authority_list_name
    |   rule_authority_list_name
    ;

rule_authority_list_name: tokenSTRING
    {
        if (asAsgRuleAuthorityAdd(yyAsgRule, $1))
            yyerror("");
        free((void *)$1);
    }
    ;

/* YAML Content processing rules */

yaml_content: tokenYAML_VERSION yaml_versioned_content
    {
        if(strcmp($1, "1.0") != 0) {
            yyerror("Unsupported YAML version");
        }
        free((void *)$1);
    }
    ;


yaml_versioned_content: yaml_versioned_content yaml_content_item
    |   yaml_content_item
    ;

yaml_content_item: yaml_uags
    |   yaml_hags
    |   yaml_asgs
    ;


yaml_uags: tokenUAG yaml_uag_list
    ;

yaml_hags: tokenHAG yaml_hag_list
    ;

yaml_asgs: tokenASG yaml_asg_list
    ;


yaml_uag_list: yaml_uag_list yaml_uag_item
    |   yaml_uag_item
    ;

yaml_hag_list: yaml_hag_list yaml_hag_item
    |   yaml_hag_item
    ;

yaml_asg_list: yaml_asg_list yaml_asg_item
    |   yaml_asg_item
    ;


yaml_uag_item: yaml_uag_name yaml_uag_users
    ;

yaml_hag_item: yaml_hag_name yaml_hag_hosts
    ;

yaml_asg_item: yaml_asg_name yaml_asg_configuration
    ;


yaml_asg_configuration: yaml_asg_configuration yaml_asg_configuration_item
    |   yaml_asg_configuration_item
    ;

yaml_asg_configuration_item: yaml_asg_links
    |   yaml_asg_rules
    ;


yaml_uag_name: tokenYAML_NAME
    {
        yyUag = asUagAdd($1);
        if(!yyUag) yyerror("Unable to add UAG");
        free((void *)$1);
    }
    ;

yaml_hag_name: tokenYAML_NAME
    {
        yyHag = asHagAdd($1);
        if(!yyHag) yyerror("Unable to add HAG");
        free((void *)$1);
    }
    ;

yaml_asg_name: tokenYAML_NAME
    {
        yyAsg = asAsgAdd($1);
        if(!yyAsg) yyerror("Unable to add ASG");
        free((void *)$1);
    }
    ;


yaml_uag_users: tokenYAML_USERS yaml_uag_users_list
    ;

yaml_hag_hosts: tokenYAML_HOSTS yaml_hag_hosts_list
    ;

yaml_asg_links: tokenYAML_LINKS yaml_asg_links_list
    ;

yaml_asg_rules: tokenYAML_RULES yaml_asg_rules_list
    ;


yaml_uag_users_list: yaml_uag_users_list yaml_user_item
    |   yaml_user_item
    ;

yaml_hag_hosts_list: yaml_hag_hosts_list yaml_host_item
    |   yaml_host_item
    ;

yaml_asg_links_list: yaml_asg_links_list yaml_link
    |   yaml_link
    ;

yaml_asg_rules_list: yaml_asg_rules_list yaml_rule
    |   yaml_rule
    ;


yaml_user_item: tokenSTRING
    {
        if (asUagAddUser(yyUag, $1))
            yyerror("Unable to add user to UAG");
        free((void *)$1);
    }
    ;

yaml_host_item: tokenSTRING
    {
        if (asHagAddHost(yyHag, $1))
            yyerror("Unable to add host to HAG");
        free((void *)$1);
    }
    ;

yaml_link: tokenYAML_INP
    {
        if (asAsgAddInp(yyAsg,$1,yyInp))
            yyerror("Unable to add link to ASG");
        free((void *)$1);
    }
    ;

yaml_rule: yaml_rule_head yaml_rule_option yaml_rule_body
    |   yaml_rule_head yaml_rule_body
    |   yaml_rule_head
    ;

yaml_rule_head: tokenYAML_LEVEL tokenYAML_ACCESS
    {
        yyAsgRule = asAsgAddRule(yyAsg, $2, $1);
        if (!yyAsgRule) {
            yyerror("Failed to add rule");
            }
    }
    ;

yaml_rule_option: yaml_rule_option tokenYAML_TRAPWRITE
    {
        if (asAsgAddRuleOptions(yyAsgRule,AS_TRAP_WRITE))
            yyerror("Unable to add trapwrite option to the ASG rule");
    } 
    |   yaml_rule_option tokenYAML_ISTLS
    {
        if (asAsgAddRuleTLSOption(yyAsgRule,1))
            yyerror("Unable to add isTLS option to the ASG rule");
    }
    |   yaml_rule_option tokenYAML_ISNOTTLS
    {
        if (asAsgAddRuleTLSOption(yyAsgRule,0))
            yyerror("Unable to add isTLS option to the ASG rule");
    }
    |   tokenYAML_TRAPWRITE
    {
        if (asAsgAddRuleOptions(yyAsgRule,AS_TRAP_WRITE))
            yyerror("Unable to add trapwrite option to the ASG rule");
    } 
    |   tokenYAML_ISTLS
    {
        if (asAsgAddRuleTLSOption(yyAsgRule,1))
            yyerror("Unable to add isTLS option to the ASG rule");
    }
    |   tokenYAML_ISNOTTLS
    {
        if (asAsgAddRuleTLSOption(yyAsgRule,0))
            yyerror("Unable to add isTLS option to the ASG rule");
    }
    ;

yaml_rule_body: yaml_rule_body yaml_rule_item
    |   yaml_rule_item
    ;

yaml_rule_item: yaml_rule_calc
    |   yaml_rule_uags
    |   yaml_rule_hags
    |   yaml_rule_methods
    |   yaml_rule_authoritys
    ;

yaml_rule_calc: tokenYAML_CALC
    {
        if (asAsgRuleCalc(yyAsgRule,$1))
            yyerror("Unable to add calc to the ASG rule");
        free((void *)$1);
    }
    ;

yaml_rule_uags:  tokenUAG yaml_rule_uag_list
    ;

yaml_rule_hags:  tokenHAG yaml_rule_hag_list
    ;

yaml_rule_methods:  tokenMETHOD yaml_rule_method_list
    ;

yaml_rule_authoritys:  tokenAUTHORITY yaml_rule_authority_list
    ;



yaml_rule_uag_list: yaml_rule_uag_list yaml_rule_uag_item
    |   yaml_rule_uag_item
    ;

yaml_rule_hag_list: yaml_rule_hag_list yaml_rule_hag_item
    |   yaml_rule_hag_item
    ;

yaml_rule_method_list: yaml_rule_method_list yaml_rule_method_item
    |   yaml_rule_method_item
    ;

yaml_rule_authority_list: yaml_rule_authority_list yaml_rule_authority_item
    |   yaml_rule_authority_item
    ;


yaml_rule_uag_item: tokenSTRING
    {
        if (asAsgRuleUagAdd(yyAsgRule,$1))
            yyerror("Unable to add UAG constraint to the ASG rule");
        free((void *)$1);
    }
    ;

yaml_rule_hag_item: tokenSTRING
    {
        if (asAsgRuleHagAdd(yyAsgRule,$1))
            yyerror("Unable to add HAG constraint to the ASG rule");
        free((void *)$1);
    }
    ;

yaml_rule_method_item: tokenSTRING
    {
        if (asAsgRuleMethodAdd(yyAsgRule,$1))
            yyerror("Unable to add authentication method constraint to the ASG rule");
        free((void *)$1);
    }
    ;

yaml_rule_authority_item: tokenSTRING
    {
        if (asAsgRuleAuthorityAdd(yyAsgRule,$1))
            yyerror("Unable to add authentication authority constraint to the ASG rule");
        free((void *)$1);
    }
    ;

%%

#include "asLib_lex.c"

static int yyerror(char *str)
{
    if (strlen(str))
        errlogPrintf("%s at line %d\n", str, line_num);
    else
        errlogPrintf(ERL_ERROR " at line %d\n", line_num);
    yyFailed = TRUE;
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
        yyreset();
        yyrestart(NULL);
    }
    FirstFlag = 0;
    rtnval = yyparse();
    if(rtnval!=0 || yyFailed) return(-1); else return(0);
}
