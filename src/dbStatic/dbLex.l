name  [a-zA-Z0-9_\-:\.\[\]<>;]
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
string {regular}|{spaces}|{escape}


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
	yylval.Str = (char *)dbmfMalloc(strlen(yytext)+1);
	strcpy(yylval.Str,yytext);
	return(tokenSTRING); 
	}

([0-9]+|([0-9]*\.[0-9]+)([eE][-+]?[0-9]+)?) { /*real number*/
	yylval.Str = (char *)dbmfMalloc(strlen(yytext)+1);
	strcpy(yylval.Str,yytext);
	return(tokenSTRING); 
}


{name}+ { /*unquoted string*/
	yylval.Str = (char *)dbmfMalloc(strlen(yytext)+1);
	strcpy(yylval.Str,yytext);
	return(tokenSTRING); 
}

\"{string}*\" { /*quoted string*/
	yylval.Str = (char *)dbmfMalloc(strlen(yytext)+1);
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
		YY_BUFFER_STATE *dummy=0;
	  
		sprintf(message,"invalid character '%c'",yytext[0]);
	  	yyerror(message);
		/*The following suppresses compiler warning messages*/
		if(FALSE) yyunput('c',message);
		if(FALSE) yy_switch_to_buffer(*dummy);
	}

%%
