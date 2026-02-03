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

static ELLLIST yyCertPath;
typedef struct {
    ELLNODE node;
    char *commonName;
} CertPathNode_t;

static int saveAuthorityEntry(char *name);

static int pushCertPath(char *commonName);
static void popCertPath();

static char *getCurrentCertPath();

static void initCertPathStack();
static void freeCertPathStack();

static int _pushCertPath(const char *commonName);
static char* yystrdup(const char *inp);
%}

%start asconfig

%token tokenUAG tokenHAG tokenASG tokenRULE tokenCALC tokenMETHOD tokenAUTHORITY tokenPROTOCOL
%token <Str> tokenSTRING
%token <Int64> tokenINT64 tokenINP
%token <Float64> tokenFLOAT64

%union {
    epicsInt64 Int64;
    epicsFloat64 Float64;
    char *Str;
}

%type <Str> auth_head
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
    |   tokenAUTHORITY top_auth { popCertPath(); }
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
    | tokenMETHOD
    { $$ = yystrdup("METHOD"); }
    | tokenAUTHORITY
    { $$ = yystrdup("AUTHORITY"); }
    | tokenPROTOCOL
    { $$ = yystrdup("PROTOCOL"); }
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

top_auth: top_auth_head  auth_body
    | top_auth_head
    ;

top_auth_head:   '(' tokenSTRING ',' tokenSTRING ')'
    {
        pushCertPath($4);         // Add this new Certificate Path component to the Certificate Chain
        saveAuthorityEntry($2);   // Then create a new EPICS Security AUTHORITY with the given name
    }
    | '(' tokenSTRING ')'
    {
        pushCertPath($2);         // Add this new Certificate Path component to the Certificate Chain
    }
    ;

auth_body: '{' auth_body_item_list '}'
    ;

auth_body_item_list: auth_body_item auth_body_item_list
    | auth_body_item
    ;

auth_body_item: tokenAUTHORITY auth_head
    {
        saveAuthorityEntry($2);   // Create a new EPICS Security AUTHORITY with the given name
    } auth_body
    {
        popCertPath();
    }
    | tokenAUTHORITY auth_head
    {
        saveAuthorityEntry($2);   // Then create a new EPICS Security AUTHORITY with the given name
        popCertPath();
    }
    ;

auth_head: '(' tokenSTRING ',' tokenSTRING ')'
    {
        pushCertPath($4);
        $$ = $2;
    }
    | '(' tokenSTRING ')'
    {
        pushCertPath($2);
        $$ = NULL;
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
        } else if((strcmp($3,"RPC")==0)) {
            yyAsgRule = asAsgAddRule(yyAsg,asRPC,(int)$1);
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
    |   tokenMETHOD '(' rule_method_list ')'
    |   tokenAUTHORITY '(' rule_authority_list ')'
    |   tokenPROTOCOL '(' tokenSTRING ')'
    {
        if((strcasecmp($3,"TLS")==0)) {
            if (asAsgAddProtocolAdd(yyAsgRule,AS_PROTOCOL_TLS))
                yyerror("");
        } else if((strcasecmp($3,"TCP")==0)) {
            if (asAsgAddProtocolAdd(yyAsgRule,AS_PROTOCOL_TCP))
                yyerror("");
        } else {
            yywarn("Ignoring RULE containing unsupported PROTOCOL", $3);
            if (asAsgRuleDisable(yyAsgRule))
                yyerror("");
        }
        free($3);
    }
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

rule_method_list: rule_method_list ',' rule_method_list_name
    |   rule_method_list_name
    ;

rule_method_list_name: tokenSTRING
    {
        if (asAsgRuleMethodAdd(yyAsgRule, $1))
            yyerror("");
        free($1);
    }
    ;

rule_authority_list: rule_authority_list ',' rule_authority_list_name
    |   rule_authority_list_name
    ;

rule_authority_list_name: tokenSTRING
    {
        if (asAsgRuleAuthorityAdd(yyAsgRule, $1))
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
    initCertPathStack();    // Initialise the Certificate Chain to store an ongoing stack of certificates as they are parsed
    rtnval = yyparse();
    freeCertPathStack();    // Free the Certificate Chain
    if(rtnval!=0 || yyFailed) return(-1); else return(0);
}

/**
 * Add the given Certificate Authority's Common Name to the Certificate Chain
 * and signal errors to parser if it fails.
 * Free up the given Common Name once consumed
 */
static int pushCertPath(char *commonName) {
    if (_pushCertPath(commonName) != 0) {
        yyerror("Out of memory");
        free(commonName);
        return -1;
    }
    free(commonName);
    return 0;
}

/**
 * Make an actual entry in EPICS Security in the list of declared named AUTHORITIES keyed on the given AUTHORITY ID.
 *
 * This will retrieve the Certificate Chain that has been parsed up till now, including
 * all parent components that have been seen, and will associate it with the given AUTHORITY ID by
 * calling `asAddAuthority` to add it to EPICS Security as a named AUTHORITY entry
 * that can be referenced in an ASG RULE.
 */
static int saveAuthorityEntry(char *name) {
    if (name) {
        char *auth_chain = getCurrentCertPath();
        if (!auth_chain) {
            yyerror("Out of memory");
            free(name);
            return -1;
        }

        if (!asAddAuthority(name, auth_chain)) {
            char message[100];
            sprintf(message, "AUTHORITY: %s=%s", name, auth_chain);
            free(auth_chain);
            free(name);
            yyerror(message);
            return -1;
        }

        free(auth_chain);
        free(name);
    }
    return 0;
}

/**
 * Add the given Certificate Authority's Common Name to the end of the current Certificate Chain
 */
static int _pushCertPath(const char *commonName) {
    CertPathNode_t *node = malloc(sizeof(CertPathNode_t));
    if (!node) return -1;

    node->commonName = strdup(commonName);
    if (!node->commonName) {
        free(node);
        return -1;
    }

    ellAdd(&yyCertPath, &node->node);
    return 0;
}

/**
 * Remove the last Common Name that was added to the Certificate Chain
 */
static void popCertPath() {
    CertPathNode_t *node = (CertPathNode_t *)ellLast(&yyCertPath);
    if (node) {
        ellDelete(&yyCertPath, &node->node);
        free(node->commonName);
        free(node);
    }
}

/**
 * Gets the current Certificate Chain that has been parsed so far.
 */
static char *getCurrentCertPath() {
    size_t total_len = 1;  /* For null terminator */
    CertPathNode_t *node;
    char *result;

    /* First pass: calculate required length */
    for (node = (CertPathNode_t *)ellFirst(&yyCertPath); node;
         node = (CertPathNode_t *)ellNext(&node->node)) {
        total_len += strlen(node->commonName) + 1;  /* +1 for newline */
    }

    result = malloc(total_len);
    if (!result) return NULL;
    result[0] = '\0';

    /* Second pass: build string */
    for (node = (CertPathNode_t *)ellFirst(&yyCertPath); node;
         node = (CertPathNode_t *)ellNext(&node->node)) {
        if (result[0]) strcat(result, "\n");
        strcat(result, node->commonName);
    }

    return result;
}

/**
 * Initialise the Certificate Chain when we start parsing
 */
static void initCertPathStack() {
    ellInit(&yyCertPath);
}

/**
 * Free up the Certificate Chain once we're done parsing
 */
static void freeCertPathStack() {
    while (ellFirst(&yyCertPath)) {
        popCertPath();
    }
}

static
char* yystrdup(const char *inp) {
    char* ret = strdup(inp);
    if(!ret)
        yyerror("MALLOC");
    return ret;
}

