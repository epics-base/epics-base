/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
b	[a-zA-Z0-9_]
a	[ \t]
d	[a-zA-Z0-9_\,\./\*#\{\}\[\]%:;!|\'\-&\(\)@\?\+<>=$\^\~]

%{
%}

%%

^[A-Z0-9_]+	{ yylval.Str=(char *)malloc(strlen(yytext)+1);
			strcpy(yylval.Str,yytext);
			return(FIELD); }

^"PV:"		{ return(PV); }
"Type:" 	{ return(TYPE); }
""		{ return(CLOSE); }
"$$end"		{ return(CLOSE); }

{d}+		{ yylval.Str=(char *)malloc(strlen(yytext)+1);
			strcpy(yylval.Str,yytext);
			return(WORD);   }

.			;
\n			{ line_num++; }

%%
