/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
newline     "\n"
backslash   "\\"
doublequote "\""
comment     "#"
whitespace  [ \t\r]
escape      {backslash}.
stringchar  [^"\n\\]

name        [a-zA-Z0-9_\-+:.\[\]<>;]
digit       [0-9]
punctuation [(){},]
link        [A-L]

/* For Yaml processing */
unquotedstringchar  [^\n]
L3          (([ ]{10}-[ ])|([ ]{12}))
L2          (([ ]{6}-[ ])|([ ]{8}))
L1          (([ ]{2}-[ ])|([ ]{4}))
read        (read|READ)
write       (write|WRITE)
rpc         (rpc|RPC)
none        (none|NONE)
true        (true|TRUE)
false       (false|FALSE)
version     [0-9]+\.[0-9]+

%{
static ASINPUTFUNCPTR *my_yyinput;
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=(*my_yyinput)((char *)b,ms))

static int yyreset(void)
{
        line_num=1;
        BEGIN INITIAL;
        return(0);
}

/* For Yaml processing */
static unsigned char *asSkipIndent(unsigned char *str) {
    while (isspace(*str) || *str == '-') str++;
    return str;
}

static unsigned char *asSkipToValue(unsigned char *str) {
    while (*str != ':' && *str) str++;
    if ( !*str ) return str;
    return str+2;
}

static unsigned char *asDupQuotedString(unsigned char *str) {
    while (*str != '"' && *str) str++;
    if ( !*str ) return asStrdup(str);
    str=(unsigned char *)asStrdup(str+1);
    str[strlen((char *)str)-1] = '\0';
    return str;
}

static unsigned char *asExtractInp(unsigned char *str) {
    str = asSkipIndent(str);
    yyInp = str[3] - 'A';
    return (unsigned char *)asStrdup(str+6);
}

%}

%x YAML_MODE

%%
^#{whitespace}*EPICS{whitespace}*(yaml|YAML) {
    BEGIN(YAML_MODE);
    return(tokenYAML_START);
}

<YAML_MODE>^(version|VERSION):[ ]{version}               { yylval.Str=asStrdup(yytext+9); return(tokenYAML_VERSION); }
<YAML_MODE>^\$schema:[ ]*\"[^\"]*\"                      ; /* Ignore schema declaration */

<YAML_MODE>^(uags|UAGS):                                 { return tokenUAG; }
<YAML_MODE>^(hags|HAGS):                                 { return tokenHAG; }
<YAML_MODE>^(asgs|ASGS):                                 { return tokenASG; }

<YAML_MODE>{L1}(hosts|HOSTS):                            { return tokenYAML_HOSTS; }
<YAML_MODE>{L1}(users|USERS):                            { return tokenYAML_USERS; }
<YAML_MODE>{L1}(rules|RULES):                            { return tokenYAML_RULES; }
<YAML_MODE>{L1}(links|LINKS):                            { return tokenYAML_LINKS; }
<YAML_MODE>{L2}(uags|UAGS):                              { return tokenUAG; }
<YAML_MODE>{L2}(hags|HAGS):                              { return tokenHAG; }
<YAML_MODE>{L2}(methods|METHODS):                        { return tokenMETHOD; }
<YAML_MODE>{L2}(authorities|AUTHORITIES):                { return tokenAUTHORITY; }

<YAML_MODE>{L1}(name|NAME):[ ]{unquotedstringchar}+      { yylval.Str=asStrdup(asSkipToValue(yytext)); return tokenYAML_NAME; }
<YAML_MODE>{L2}(level|LEVEL):[ ]{digit}+                 { yylval.Int = atoi((unsigned char *)asSkipToValue(yytext)); return tokenYAML_LEVEL; }
<YAML_MODE>{L2}(access|ACCESS):[ ]{read}                 { yylval.Int = asREAD; return tokenYAML_ACCESS; }
<YAML_MODE>{L2}(access|ACCESS):[ ]{write}                { yylval.Int = asWRITE; return tokenYAML_ACCESS; }
<YAML_MODE>{L2}(access|ACCESS):[ ]{rpc}                  { yylval.Int = asRPC; return tokenYAML_ACCESS; }
<YAML_MODE>{L2}(access|ACCESS):[ ]{none}                 { yylval.Int = asNOACCESS; return tokenYAML_ACCESS; }
<YAML_MODE>{L2}(calc|CALC):[ ]{stringchar}+              { yylval.Str=asStrdup(asSkipToValue(yytext)); return tokenYAML_CALC; }
<YAML_MODE>{L2}(isTLS|ISTLS):[ ]{true}                   { return tokenYAML_ISTLS; }
<YAML_MODE>{L2}(isTLS|ISTLS):[ ]{false}                  { return tokenYAML_ISNOTTLS; }
<YAML_MODE>{L2}(trapwrite|TRAPWRITE):[ ]{true}           { return tokenYAML_TRAPWRITE; }
<YAML_MODE>{L2}(trapwrite|TRAPWRITE):[ ]{false}          ;

<YAML_MODE>{L2}(inp|INP){link}:[ ]{unquotedstringchar}+  {
        yylval.Str = asExtractInp(yytext);
        return(tokenYAML_INP);
}

<YAML_MODE>({L2}|{L3}){doublequote}({stringchar}|{escape})*{doublequote}
                                                         { yylval.Str=asDupQuotedString(yytext); return(tokenSTRING); }
<YAML_MODE>({L2}|{L3}){unquotedstringchar}+              { yylval.Str=asStrdup(asSkipIndent(yytext)); return(tokenSTRING); }
<YAML_MODE>{newline}       { line_num++; }

<YAML_MODE>{comment}.*     ;
<YAML_MODE>{whitespace}    ;

ASG       { return(tokenASG);       }
AUTHORITY { return(tokenAUTHORITY); }
CALC      { return(tokenCALC);      }
RPC       { return(tokenRPC);       }
HAG       { return(tokenHAG);       }
METHOD    { return(tokenMETHOD);    }
NONE      { return(tokenNONE);      }
READ      { return(tokenREAD);      }
RULE      { return(tokenRULE);      }
UAG       { return(tokenUAG);       }
WRITE     { return(tokenWRITE);     }

INP{link} {
        yylval.Int = yytext[3];
        yylval.Int -= 'A';
        return(tokenINP);
}

{digit}+ { /*integer*/
        yylval.Int = atoi((char *)yytext);
        return(tokenINTEGER);
}

{name}+ { /*unquoted string*/
        yylval.Str=asStrdup(yytext);
        return(tokenSTRING);
}

{doublequote}({stringchar}|{escape})*{doublequote} { /* quoted string */
        yylval.Str=asStrdup(yytext+1);
        yylval.Str[strlen(yylval.Str)-1] = '\0';
        return(tokenSTRING);
}

{doublequote}({stringchar}|{escape})*{newline} { /* bad string */
        yyerror("Newline in quoted string, closing quote missing");
}

{punctuation}   { return(yytext[0]); }

{newline}       { line_num++; }

{comment}.*     ;
{whitespace}    ;

.       {
    char message[40];
    YY_BUFFER_STATE *dummy=0;

    if (isprint((int) yytext[0])) {
        sprintf(message, "Invalid character '%c'", yytext[0]);
    }
    else {
        sprintf(message, "Invalid character 0x%2.2x", yytext[0]);
    }
    yyerror(message);

    /*The following suppress compiler warning messages*/
    if (0) yyunput('c',(unsigned char *) message);
    if (0) yy_switch_to_buffer(*dummy);
}

%%
