name  [a-zA-Z0-9_\-:\.\[\]<>;]
string   [a-zA-Z0-9_\,\^~\./\*#\[\]%: ;!|\'\-&\(\)@\?\+<>=\$\t]

%{
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=(*db_yyinput)(b,ms))

static int yyreset(void)
{
	BEGIN INITIAL;
	return(0);
}

%}

%%

"include"	{return(tokenINCLUDE);}
"path"		{return(tokenPATH);}
"addpath"	{return(tokenADDPATH);}
"menu"		{return(tokenMENU);}
"choice"	{return(tokenCHOICE);}
"recordtype"	{return(tokenRECORDTYPE);}
"field"		{return(tokenFIELD);}
"device"	{return(tokenDEVICE);}
"driver"	{return(tokenDRIVER);}
"breaktable"	{return(tokenBREAKTABLE);}
"record"	{return(tokenRECORD);}
"grecord"	{return(tokenGRECORD);}

[0-9]+ { /*integer number*/
	yylval.Str = (char *)malloc(strlen(yytext)+1);
	strcpy(yylval.Str,yytext);
	return(tokenSTRING); 
	}

([0-9]+|([0-9]*\.[0-9]+)([eE][-+]?[0-9]+)?) { /*real number*/
	yylval.Str = (char *)malloc(strlen(yytext)+1);
	strcpy(yylval.Str,yytext);
	return(tokenSTRING); 
}


{name}+ { /*unquoted string*/
	yylval.Str = (char *)malloc(strlen(yytext)+1);
	strcpy(yylval.Str,yytext);
	return(tokenSTRING); 
}

\"{string}*\" { /*quoted string*/
	yylval.Str = (char *)malloc(strlen(yytext)+1);
	strcpy(yylval.Str,yytext+1);
	yylval.Str[strlen(yylval.Str)-1] = '\0';
	return(tokenSTRING); 
}

"{"     { return(yytext[0]);      }
"}"     { return(yytext[0]);      }
"("     { return(yytext[0]);      }
")"     { return(yytext[0]);      }
","	{ return(yytext[0]);	}
#.*	{ ;}
[ \t\r]	;
\n      { ;}
.       { 
		char message[20];
	  
		sprintf(message,"invalid character '%c'",yytext[0]);
	  	yyerror(message);
	}

%%
