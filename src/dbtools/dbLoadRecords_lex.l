
pvname	[a-zA-Z0-9_~\-:\.\[\]<>;]
value	[a-zA-Z0-9_\,\^~\./\*#\[\]%: ;!|\'\-&\(\)@\?\+<>=\$\{\}]

%{
%}

%%

\#.*\n	;

"field"	    { return(FIELD); }
"grecord"    { return(RECORD); }
"record"    { return(RECORD); }
"database"  {  return(DATABASE); }
"nowhere"   {  return(NOWHERE); }
"application"   {  return(APPL); }

\"{value}*\"    { yylval.Str=(char *)dbmfMalloc(handle,strlen(yytext)+1); strcpy(yylval.Str,yytext+1); yylval.Str[strlen(yylval.Str)-1] = '\0'; return(VALUE); }

{pvname}+		{ yylval.Str=(char *)dbmfMalloc(handle,strlen(yytext)+1); strcpy(yylval.Str,yytext); return(WORD);	}

"{"	{ return(O_BRACE);	}
"}"	{ return(C_BRACE);	}
"("	{ return(O_PAREN);	}
")"	{ return(C_PAREN);	}
","	{ return(COMMA);	}

.	;
\n	{ line_num ++;}

%%
