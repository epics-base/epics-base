/*************************************************************************\
* Copyright (c) 2006 UChicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

newline     "\n"
backslash   "\\"
doublequote "\""
singlequote "'"
comment     "#"
whitespace  [ \t\r]
escape      {backslash}.
dstringchar [^"\n\\]
sstringchar [^'\n\\]
bareword    [a-zA-Z0-9_\-+:./\\\[\]<>;]

%%

"pattern" { return(PATTERN);	}
"file"    { return(DBFILE);	}

{doublequote}({dstringchar}|{escape})*{doublequote} |
{singlequote}({sstringchar}|{escape})*{singlequote} {
	yylval.Str = dbmfStrdup(yytext+1);
	yylval.Str[strlen(yylval.Str)-1] = '\0';
	return(QUOTE);
    }

{bareword}+ {
	yylval.Str = dbmfStrdup(yytext);
	return(WORD);
    }

"="	{ return(EQUALS);	}
","	{ return(COMMA);	}
"{"	{ return(O_BRACE);	}
"}"	{ return(C_BRACE);	}

{comment}.*	;
{whitespace}	;
{newline}	{ line_num++;	}

. {
	char message[40];

	sprintf(message,"invalid character '%c'", yytext[0]);
	yyerror(message);

	/* Suppress compiler warning messages */
	if (0) yyunput('c',NULL);
	if (0) yy_switch_to_buffer(NULL);
    }

%%
