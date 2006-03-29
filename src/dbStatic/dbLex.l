/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
name  [a-zA-Z0-9_\-:\.\[\]<>;]
notquote [^\"]
escapequote \\\"
string {notquote}|{escapequote}
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
"info"		return(tokenINFO);
"registrar"	return(tokenREGISTRAR);
"function"	return(tokenFUNCTION);
"variable"	return(tokenVARIABLE);

[0-9]+ { /*integer number*/
	yylval.Str = dbmfStrdup(yytext);
	return(tokenSTRING); 
}

-?(([0-9]+)|([0-9]*\.[0-9]+)([eE][-+]?[0-9]+)?) { /*real number*/
	yylval.Str = dbmfStrdup(yytext);
	return(tokenSTRING); 
}


{name}+ { /*unquoted string*/
	yylval.Str = dbmfStrdup(yytext);
	return(tokenSTRING); 
}

\"{string}*\" { /*quoted string*/
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
\#.*	;
[ \t\r]	;
\n	;

.	{ 
	char message[20];
	YY_BUFFER_STATE *dummy=0;

	sprintf(message,"invalid character '%c'",yytext[0]);
	yyerror(message);
	/*The following suppresses compiler warning messages*/
	if(FALSE) yyunput('c',(unsigned char *) message);
	if(FALSE) yy_switch_to_buffer(*dummy);
}

%%
