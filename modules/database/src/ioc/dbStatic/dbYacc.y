/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
%{
static int yyerror();
static int yy_start;
static long pvt_yy_parse(void);
static int yyFailed = 0;
static int yyAbort = 0;
#include "dbLexRoutines.c"
%}

%start database

%union
{
    char    *Str;
}

%token tokenINCLUDE tokenPATH tokenADDPATH
%token tokenALIAS tokenMENU tokenCHOICE tokenRECORDTYPE
%token tokenFIELD tokenINFO tokenREGISTRAR
%token tokenDEVICE tokenDRIVER tokenLINK tokenBREAKTABLE
%token tokenRECORD tokenGRECORD tokenVARIABLE tokenFUNCTION
%token <Str> tokenSTRING tokenCDEFS

%token jsonNULL jsonTRUE jsonFALSE
%token <Str> jsonNUMBER jsonSTRING jsonBARE
%type <Str> json_value json_string json_object json_array
%type <Str> json_members json_pair json_key json_elements

%%

database:   /* empty */
    |   database_item_list
    ;

database_item_list: database_item_list database_item
    |   database_item
    ;

database_item:  include
    |   path
    |   addpath
    |   tokenMENU menu_head menu_body
    |   tokenRECORDTYPE recordtype_head recordtype_body
    |   device
    |   driver
    |   link
    |   registrar
    |   function
    |   variable
    |   tokenBREAKTABLE break_head break_body
    |   tokenRECORD record_head record_body
    |   tokenGRECORD grecord_head record_body
    |   alias
    ;

include:    tokenINCLUDE tokenSTRING
{
    if(dbStaticDebug>2) printf("include : %s\n",$2);
    dbIncludeNew($2); dbmfFree($2);
};

path:   tokenPATH tokenSTRING
{
    if(dbStaticDebug>2) printf("path : %s\n",$2);
    dbPathCmd($2); dbmfFree($2);
};

addpath:    tokenADDPATH tokenSTRING
{
    if(dbStaticDebug>2) printf("addpath : %s\n",$2);
    dbAddPathCmd($2); dbmfFree($2);
};

menu_head:  '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("menu_head %s\n",$2);
    dbMenuHead($2); dbmfFree($2);
};

menu_body:  '{' choice_list '}'
{
    if(dbStaticDebug>2) printf("menu_body\n");
    dbMenuBody();
};

choice_list:    choice_list choice | choice;

choice: tokenCHOICE '(' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("choice %s %s\n",$3,$5);
    dbMenuChoice($3,$5); dbmfFree($3); dbmfFree($5);
}
    | include;

recordtype_head: '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("recordtype_head %s\n",$2);
    dbRecordtypeHead($2); dbmfFree($2);
};

recordtype_body: '{' '}'
{
    if(dbStaticDebug>2) printf("empty recordtype_body\n");
    dbRecordtypeEmpty();
}
    | '{' recordtype_field_list '}'
{
    if(dbStaticDebug>2) printf("recordtype_body\n");
    dbRecordtypeBody();
};

recordtype_field_list:  recordtype_field_list recordtype_field
    | recordtype_field;

recordtype_field: tokenFIELD recordtype_field_head recordtype_field_body
    | tokenCDEFS
{
    if(dbStaticDebug>2) printf("recordtype_cdef %s", $1);
    dbRecordtypeCdef($1); dbmfFree($1);
}
    | include ;

recordtype_field_head:  '(' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("recordtype_field_head %s %s\n",$2,$4);
    dbRecordtypeFieldHead($2,$4); dbmfFree($2); dbmfFree($4);
};

recordtype_field_body:  '{' recordtype_field_item_list '}' ;

recordtype_field_item_list:  recordtype_field_item_list recordtype_field_item
    | recordtype_field_item;

recordtype_field_item:  tokenSTRING '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("recordtype_field_item %s %s\n",$1,$3);
    dbRecordtypeFieldItem($1,$3); dbmfFree($1); dbmfFree($3);
}
    | tokenMENU '(' tokenSTRING ')'
{

    if(dbStaticDebug>2) printf("recordtype_field_item %s (%s)\n","menu",$3);
    dbRecordtypeFieldItem("menu",$3); dbmfFree($3);
};


device: tokenDEVICE '('
    tokenSTRING ',' tokenSTRING ',' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("device %s %s %s %s\n",$3,$5,$7,$9);
    dbDevice($3,$5,$7,$9);
    dbmfFree($3); dbmfFree($5);
    dbmfFree($7); dbmfFree($9);
};


driver: tokenDRIVER '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("driver %s\n",$3);
    dbDriver($3); dbmfFree($3);
};

link: tokenLINK '(' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("link %s %s\n",$3,$5);
    dbLinkType($3,$5);
    dbmfFree($3); dbmfFree($5);
};

registrar: tokenREGISTRAR '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("registrar %s\n",$3);
    dbRegistrar($3); dbmfFree($3);
};

function: tokenFUNCTION '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("function %s\n",$3);
    dbFunction($3); dbmfFree($3);
};

variable: tokenVARIABLE '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("variable %s\n",$3);
    dbVariable($3,"int"); dbmfFree($3);
}
        | tokenVARIABLE '(' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("variable %s, %s\n",$3,$5);
    dbVariable($3,$5); dbmfFree($3); dbmfFree($5);
};

break_head: '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("break_head %s\n",$2);
    dbBreakHead($2); dbmfFree($2);
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
    dbBreakItem($1); dbmfFree($1);
};


grecord_head: '(' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("grecord_head %s %s\n",$2,$4);
    dbRecordHead($2,$4,1); dbmfFree($2); dbmfFree($4);
};

record_head: '(' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("record_head %s %s\n",$2,$4);
    dbRecordHead($2,$4,0); dbmfFree($2); dbmfFree($4);
};

record_body: /* empty */
{
    if(dbStaticDebug>2) printf("null record_body\n");
    dbRecordBody();
}
    | '{' '}'
{
    if(dbStaticDebug>2) printf("empty record_body\n");
    dbRecordBody();
}
        | '{' record_field_list '}'
{
    if(dbStaticDebug>2) printf("record_body\n");
    dbRecordBody();
};

record_field_list:  record_field_list record_field
    |   record_field;

record_field: tokenFIELD '(' tokenSTRING ','
    { BEGIN JSON; } json_value { BEGIN INITIAL; } ')'
{
    if(dbStaticDebug>2) printf("record_field %s %s\n",$3,$6);
    dbRecordField($3,$6); dbmfFree($3); dbmfFree($6);
}
    | tokenINFO '(' tokenSTRING ','
    { BEGIN JSON; } json_value { BEGIN INITIAL; } ')'
{
    if(dbStaticDebug>2) printf("record_info %s %s\n",$3,$6);
    dbRecordInfo($3,$6); dbmfFree($3); dbmfFree($6);
}
    | tokenALIAS '(' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("record_alias %s\n",$3);
    dbRecordAlias($3); dbmfFree($3);
}
    | include ;

alias: tokenALIAS '(' tokenSTRING ',' tokenSTRING ')'
{
    if(dbStaticDebug>2) printf("alias %s %s\n",$3,$5);
    dbAlias($3,$5); dbmfFree($3); dbmfFree($5);
};

json_object: '{' '}'
{
    $$ = dbmfStrdup("{}");
    if (dbStaticDebug>2) printf("json %s\n", $$);
}
    | '{' json_members '}'
{
    $$ = dbmfStrcat3("{", $2, "}");
    dbmfFree($2);
    if (dbStaticDebug>2) printf("json %s\n", $$);
};

json_members: json_pair
    | json_pair ','
    | json_pair ',' json_members
{
    $$ = dbmfStrcat3($1, ",", $3);
    dbmfFree($1); dbmfFree($3);
    if (dbStaticDebug>2) printf("json %s\n", $$);
};

json_pair: json_key ':' json_value
{
    $$ = dbmfStrcat3($1, ":", $3);
    dbmfFree($1); dbmfFree($3);
    if (dbStaticDebug>2) printf("json %s\n", $$);
};

json_key: jsonSTRING
    | jsonBARE
{
    /* A key containing any of these characters must be quoted for YAJL */
    if (strcspn($1, "+-.") < strlen($1)) {
        $$ = dbmfStrcat3("\"", $1, "\"");
        dbmfFree($1);
    }
    if (dbStaticDebug>2) printf("json %s\n", $$);
};

json_string: jsonSTRING
    | jsonBARE
{
    $$ = dbmfStrcat3("\"", $1, "\"");
    dbmfFree($1);
    if (dbStaticDebug>2) printf("json %s\n", $$);
};

json_array: '[' ']'
{
    $$ = dbmfStrdup("[]");
    if (dbStaticDebug>2) printf("json %s\n", $$);
}
    | '[' json_elements ']'
{
    $$ = dbmfStrcat3("[", $2, "]");
    dbmfFree($2);
    if (dbStaticDebug>2) printf("json %s\n", $$);
};

json_elements: json_value
    | json_value ','
{   /* Retain the trailing ',' so link parser can distinguish a
     * 1-element const list from a PV name (commas are illegal)
     */
    $$ = dbmfStrcat3($1, ",", "");
    dbmfFree($1);
    if (dbStaticDebug>2) printf("json %s\n", $$);
};
    | json_value ',' json_elements
{
    $$ = dbmfStrcat3($1, ",", $3);
    dbmfFree($1); dbmfFree($3);
    if (dbStaticDebug>2) printf("json %s\n", $$);
};

json_value: jsonNULL    { $$ = dbmfStrdup("null"); }
    | jsonTRUE  { $$ = dbmfStrdup("true"); }
    | jsonFALSE { $$ = dbmfStrdup("false"); }
    | jsonNUMBER
    | json_string
    | json_array
    | json_object ;


%%

#include "dbLex.c"


static int yyerror(char *str)
{
    if (str)
        epicsPrintf("Error: %s\n", str);
    else
        epicsPrintf("Error");
    if (!yyFailed) {    /* Only print this stuff once */
        epicsPrintf(" at or before '%s'", yytext);
        dbIncludePrint();
        yyFailed = TRUE;
    }
    return(0);
}
static long pvt_yy_parse(void)
{
    static int  FirstFlag = 1;
    long        rtnval;

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
