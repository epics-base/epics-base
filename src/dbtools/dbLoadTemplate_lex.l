
value	[a-zA-Z0-9_\,\./\*#\[\]%: ;!|\-&\(\)@\?\+<>=\$]
word	[a-zA-Z0-9_\.\^~/\*#\[\]%:;!|\-&\$\(\)@\?\+<>]
par [\"\']

%{
%}

%%

"pattern"   { return(PATTERN); }
"file"   { return(DBFILE); }
"="			{ return(EQUALS); }

{par}{value}*{par}    { yylval.Str=(char *)malloc(strlen(yytext)+1); strcpy(yylval.Str,yytext+1); yylval.Str[strlen(yylval.Str)-1] = '\0'; return(QUOTE); }

{word}+	{ yylval.Str=(char *)malloc(strlen(yytext)+1); strcpy(yylval.Str,yytext); return(WORD);	}

"("	{ return(O_PAREN);	}
")"	{ return(C_PAREN);	}
"{"	{ return(O_BRACE);	}
"}"	{ return(C_BRACE);	}

.	;
\n	{ line_num ++;}

%%
