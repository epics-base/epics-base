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
