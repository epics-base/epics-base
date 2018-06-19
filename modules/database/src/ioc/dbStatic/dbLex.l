/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
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

punctuation	[:,\[\]{}]
normalchar	[^"\\\0-\x1f]
barechar	[a-zA-Z0-9_\-+.]
escapedchar	({backslash}["\\/bfnrt])
hexdigit	[0-9a-fA-F]
unicodechar	({backslash}"u"{hexdigit}{4})
jsonchar	({normalchar}|{escapedchar}|{unicodechar})
jsondqstr	({doublequote}{jsonchar}*{doublequote})
int	("-"?([0-9]|[1-9][0-9]+))
frac	("."[0-9]+)
exp	([eE][+-]?[0-9]+)
number	({int}{frac}?{exp}?)

%{
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=(*db_yyinput)((char *)b,ms))

static int yyreset(void)
{
	BEGIN INITIAL;
	return(0);
}

%}

%x JSON

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
"link"		return(tokenLINK);
"breaktable"	return(tokenBREAKTABLE);
"record"	return(tokenRECORD);
"grecord"	return(tokenGRECORD);
"alias" 	return(tokenALIAS);
"info"		return(tokenINFO);
"registrar"	return(tokenREGISTRAR);
"function"	return(tokenFUNCTION);
"variable"	return(tokenVARIABLE);

{bareword}+ { /* unquoted string or number */
	yylval.Str = dbmfStrdup((char *) yytext);
	return(tokenSTRING); 
}

{doublequote}({stringchar}|{escape})*{doublequote} { /* quoted string */
	yylval.Str = dbmfStrdup((char *) yytext+1);
	yylval.Str[strlen(yylval.Str)-1] = '\0';
	return(tokenSTRING);
}

%.*	{ /*C definition in recordtype*/
	yylval.Str = dbmfStrdup((char *) yytext+1);
	return(tokenCDEFS);
}

"{"	return(yytext[0]);
"}"	return(yytext[0]);
"("	return(yytext[0]);
")"	return(yytext[0]);
","	return(yytext[0]);

{doublequote}({stringchar}|{escape})*{newline} { /* bad string */
	yyerrorAbort("Newline in string, closing quote missing");
}

<JSON>"null"	return jsonNULL;
<JSON>"true"	return jsonTRUE;
<JSON>"false"	return jsonFALSE;

<JSON>{punctuation}	return yytext[0];

<JSON>{jsondqstr} {
	yylval.Str = dbmfStrdup((char *) yytext);
	return jsonSTRING;
}

<JSON>{number} {
	yylval.Str = dbmfStrdup((char *) yytext);
	return jsonNUMBER;
}

<JSON>{barechar}+ {
	yylval.Str = dbmfStrdup((char *) yytext);
	return jsonBARE;
}

<INITIAL,JSON>{comment}.*	;

<INITIAL,JSON>{whitespace}	;

<INITIAL,JSON>.	{
	char message[40];
	YY_BUFFER_STATE *dummy=0;

	if (isprint((int) yytext[0])) {
	    sprintf(message, "Invalid character '%c'", yytext[0]);
	}
	else {
	    sprintf(message, "Invalid character 0x%2.2x", yytext[0]);
	}
	yyerrorAbort(message);
	/*The following suppresses compiler warning messages*/
	if(FALSE) yyunput('c',(unsigned char *) message);
	if(FALSE) yy_switch_to_buffer(*dummy);
}

%%
