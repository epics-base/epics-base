integer	[0-9]
name	[a-zA-Z0-9_]
pvname  [a-zA-Z0-9_:\.\[\]<>]
string   [a-zA-Z0-9_\,\./\*#\[\]%: ;!|\'\-&\(\)@\?\+<>=\$]

%{
static ASINPUTFUNCPTR *my_yyinput;
#undef YY_INPUT
#define YY_INPUT(b,r,ms) (r=(*my_yyinput)(b,ms))

yyreset()
{
	line_num=1;
	BEGIN INITIAL;
	return(0);
}

%}

%%

UAG	{return(tokenUAG);}
LAG	{return(tokenLAG);}
ASG	{return(tokenASG);}
LEVEL	{return(tokenLEVEL);}
CALC	{return(tokenCALC);}
INP[A-L] {/* If A-L is changed then ASMAXINP must also be changed*/
	yylval.Int = (unsigned char)yytext[3];
	yylval.Int -= 'A';
	return(tokenINP);
	}

{integer}+ {
	sscanf(yytext,"%d",&yylval.Int);
	return(tokenINTEGER);
	}

{name}+ {
	yylval.Str=(char *)asCalloc(1,strlen(yytext)+1);
	strcpy(yylval.Str,yytext);
	return(tokenNAME);
	}

{pvname}+ {
        yylval.Str=(char *)asCalloc(1,strlen(yytext)+1);
        strcpy(yylval.Str,yytext);
        return(tokenPVNAME);
    }

\"{string}*\" {
        yylval.Str=(char *)asCalloc(1,strlen(yytext)+1);
        /* making sure that neither double quote gets passed back */
        strcpy(yylval.Str,yytext+1);
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
	  
		sprintf(message,"invalid character '%c'",yytext[0]);
	  	yyerror(message);
	}

%%
