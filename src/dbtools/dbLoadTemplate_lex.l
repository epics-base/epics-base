/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

word [a-zA-Z0-9_\.\^~/\*#\[\]%:;!|\-&\$\(\)@\?\+<>\']
par [\"]

notquote [^\"]
escapequote \\\"
value {notquote}|{escapequote}

%{
%}

%%

\#.*\n	{ line_num++; }

"pattern" { return(PATTERN); }
"file"    { return(DBFILE); }
"="	  { return(EQUALS); }

{par}{value}*{par} {
	yylval.Str = dbmfStrdup(yytext+1);
	yylval.Str[strlen(yylval.Str)-1] = '\0';
	return(QUOTE);
    }

{word}+ {
	yylval.Str = dbmfStrdup(yytext);
	return(WORD);
    }

"{"	{ return(O_BRACE);	}
"}"	{ return(C_BRACE);	}

\n	{ line_num ++;}

[ \t,]

. {
	char message[20];
	YY_BUFFER_STATE *dummy=0;

	sprintf(message,"invalid character '%c'",yytext[0]);
	yyerror(message);

	/*The following suppress compiler warning messages*/
	if (0) yyunput('c',(unsigned char *) message);
	if (0) yy_switch_to_buffer(*dummy);
    }

%%
