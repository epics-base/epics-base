
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

{par}{value}*{par}    { yylval.Str = dbmfStrdup(yytext);
			yylval.Str[strlen(yylval.Str)-1] = '\0';
			return(QUOTE);
		      }

{word}+	{ yylval.Str = dbmfStrdup(yytext);
	  return(WORD);
	}

"{"	{ return(O_BRACE);	}
"}"	{ return(C_BRACE);	}

.	;
\n	{ line_num ++;}

%%
