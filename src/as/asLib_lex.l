/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
integer	[0-9]
name	[a-zA-Z0-9_\-:\.\[\]<>;]
notquote [^\"]
escapequote \\\"
string {notquote}|{escapequote}

%{
static ASINPUTFUNCPTR *my_yyinput;
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=(*my_yyinput)((char *)b,ms))

static int yyreset()
{
	line_num=1;
	BEGIN INITIAL;
	return(0);
}

%}

%%

UAG	{return(tokenUAG);}
HAG	{return(tokenHAG);}
ASG	{return(tokenASG);}
RULE	{return(tokenRULE);}
CALC	{return(tokenCALC);}
INP[A-L] {/* If A-L is changed then ASMAXINP must also be changed*/
	yylval.Int = (unsigned char)yytext[3];
	yylval.Int -= 'A';
	return(tokenINP);
	}

{integer}+ {
	yylval.Int = atoi((char *)yytext);
	return(tokenINTEGER);
	}

{name}+ { /*unquoted string*/
	yylval.Str=asStrdup(yytext);
	return(tokenSTRING);
	}

\"{string}*\" { /*quoted string*/
        /* making sure that neither double quote gets passed back */
	yylval.Str=asStrdup(yytext+1);
        yylval.Str[strlen(yylval.Str)-1] = '\0';

        return(tokenSTRING);
    }

"{"     { return(yytext[0]);      }
"}"     { return(yytext[0]);      }
"("     { return(yytext[0]);      }
")"     { return(yytext[0]);      }
","	{ return(yytext[0]);	}
^#.*
[ \t\r]	;
\n      { line_num ++;}
.       { 
		char message[20];
		YY_BUFFER_STATE *dummy=0;
	  
		sprintf(message,"invalid character '%c'",yytext[0]);
	  	yyerror(message);
		/*The following suppresses compiler warning messages*/
		if(FALSE) yyunput('c',(unsigned char *) message);
		if(FALSE) yy_switch_to_buffer(*dummy);
	}

%%
