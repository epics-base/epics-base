/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
newline     "\n"
backslash   "\\"
doublequote "\""
comment     "#"
whitespace  [ \t\r\n]
escape      {backslash}.
stringchar  [^"\n\\]
bareword    [a-zA-Z0-9_\-+:.\[\]<>;]

%{
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=(*db_yyinput)((char *)b,ms))

static int yyreset(void)
{
	BEGIN INITIAL;
	return(0);
}

%}

%%

"include"	return(tokenINCLUDE);
"path"		return(tokenPATH);
"addpath"	return(tokenADDPATH);
"menu"		return(tokenMENU);
"choice"	return(tokenCHOICE);
"recordtype"	return(tokenRECORDTYPE);
"field"		return(tokenFIELD);
"device"	return(tokenDEVICE);
"driver"	return(tokenDRIVER);
"breaktable"	return(tokenBREAKTABLE);
"record"	return(tokenRECORD);
"grecord"	return(tokenGRECORD);
"alias" 	return(tokenALIAS);
"info"		return(tokenINFO);
"registrar"	return(tokenREGISTRAR);
"function"	return(tokenFUNCTION);
"variable"	return(tokenVARIABLE);

{bareword}+ { /* unquoted string or number */
	yylval.Str = dbmfStrdup(yytext);
	return(tokenSTRING); 
}

{doublequote}({stringchar}|{escape})*{doublequote} { /* quoted string */
	yylval.Str = dbmfStrdup(yytext+1);
	yylval.Str[strlen(yylval.Str)-1] = '\0';
	return(tokenSTRING);
}

%.*	{ /*C definition in recordtype*/
	yylval.Str = dbmfStrdup(yytext+1);
	return(tokenCDEFS);
}

"{"	return(yytext[0]);
"}"	return(yytext[0]);
"("	return(yytext[0]);
")"	return(yytext[0]);
","	return(yytext[0]);

{comment}.*	;
{whitespace}	;

{doublequote}({stringchar}|{escape})*{newline} { /* bad string */
	yyerror("Newline in string, closing quote missing");
}

.	{
	char message[40];
	YY_BUFFER_STATE *dummy=0;

	sprintf(message,"Invalid character '%c'",yytext[0]);
	yyerror(message);
	/*The following suppresses compiler warning messages*/
	if(FALSE) yyunput('c',(unsigned char *) message);
	if(FALSE) yy_switch_to_buffer(*dummy);
}

%%
