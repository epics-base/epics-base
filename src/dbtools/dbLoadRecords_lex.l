
pvname	[a-zA-Z0-9_~\-:\.\[\]<>;]

BEL    \\a
BS     \\b
FF     \\f
NL     \\n
CR     \\r
HT     \\t
VT     \\v
backSla  \\\\
question \\\?
single   \\\'
double   \\\"
octal  \\[0-7]+
hexa   \\x[a-fA-F0-9]+
escape {BEL}|{BS}|{FF}|{NL}|{CR}|{HT}|{VT}|{backSla}|{question}|{single}|{double}|{octal}|{question}|{hexa}
spaces  [ \t]
regular [a-zA-Z0-9_\,\^~\./\*#\[\]%:;!|\'\-&\(\)@\?\+<>=\$\{\}]
value {regular}|{spaces}|{escape}

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

\"{value}+\" { yylval.Str=(char *)dbmfMalloc(strlen(yytext)+1);
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
