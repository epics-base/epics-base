/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

word	[a-zA-Z0-9_\.\^~/\*#\[\]%:;!|\-&\$\(\)@\?\+<>]
par [\"\']

notquote [^\"]
escapequote \\\"
value {notquote}|{escapequote}

%{
%}

%%

\#.*\n	;

"pattern" { return(PATTERN); }
"file"    { return(DBFILE); }
"="	  { return(EQUALS); }

{par}{value}*{par}    { yylval.Str=(char*)dbmfMalloc(strlen(yytext)+1);
			strcpy(yylval.Str,yytext+1);
			yylval.Str[strlen(yylval.Str)-1] = '\0';
			return(QUOTE);
		      }

{word}+	{ yylval.Str=(char*)dbmfMalloc(strlen(yytext)+1);
	  strcpy(yylval.Str,yytext);
	  return(WORD);
	}

"{"	{ return(O_BRACE);	}
"}"	{ return(C_BRACE);	}

.	;
\n	{ line_num ++;}

%%
