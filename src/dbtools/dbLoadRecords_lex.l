
pvname	[a-zA-Z0-9_~\-:\.\[\]<>;]
notquote [^\"]
escapequote \\\"
value {notquote}|{escapequote}

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


{pvname}+ { yylval.Str=(char *)dbmfMalloc(strlen(yytext)+1);
	    strcpy(yylval.Str,yytext);
	    return(WORD);
	  }

\"{value}*\" { yylval.Str=(char *)dbmfMalloc(strlen(yytext)+1);
	       yytext[strlen(yytext)-1] = '\0';
	       strcpy(yylval.Str,yytext+1);
	       return(VALUE);
	     }

"{"	{ return(O_BRACE);	}
"}"	{ return(C_BRACE);	}
"("	{ return(O_PAREN);	}
")"	{ return(C_PAREN);	}
","	{ return(COMMA);	}

.	;
\n	{ line_num ++;}

%%
