b       [a-zA-Z0-9_]
a	[ \t]
d       [a-zA-Z0-9_\,\./\*#\[\]%:;!|\'\-&\(\)@\?\+<>]

%{

yyreset()
{
	line_num=1;
	BEGIN INITIAL;
	return;
}

%}

%%

^[A-Z0-9_]+	{ yylval.Str=(char *)malloc(strlen(yytext)+1);
		strcpy(yylval.Str,yytext);
		return(FIELD); }

^"PV:"		{ return(PV); }
"Type:" 	{ return(TYPE); }
		{ return(CLOSE); }
\n		{ line_num++; return(CLOSE); }
"$$end"		{ return(CLOSE); }

{d}+		{ yylval.Str=(char *)malloc(strlen(yytext)+1);
		strcpy(yylval.Str,yytext);
		return(WORD);   }

.	;
\n	{ line_num++; }

%%
