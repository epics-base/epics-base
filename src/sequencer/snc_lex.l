%{
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)snc_lex.l	1.1	10/16/90
	ENVIRONMENT: UNIX
***************************************************************************/
/* Lexical analyzer for State Notation Compiler (snc).
 *
 * This routine recognizes State Notation Language (SNL) syntax,
 * and passes tokens to yacc().
 * All C code is passed through as a stream, without conversion.
 * Hence, the C compiler may find errors not reported by SNC.
 * Comments are recognized as part of the syntax.
 */

#include	"parse.h"
#include	"y.tab.h"	/* output from yacc */
#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif
/*#define	DEBUG 1*/

#ifdef	DEBUG
#define	RETURN(param)	{ printf("return(param)\n"); return(param); }
#else
#define	RETURN(param)	return(param);
#endif
#define	STR_BFR_SIZE	30000

	extern	int line_num; /* input line number */
	char	str_bfr[STR_BFR_SIZE]; /* holding place for strings */
	char	*str_next = str_bfr; /* current ptr to str_bfr */
	char	*c_str; /* Start of current string in str_bfr */
	int	nc;
	double	atof();
	int	one_line_c_code;

%}

/* Start conditions (SNL, C code, comment, and string) */
%Start	SNL C_CODE COMMENT STR

NAME	[a-zA-Z][a-zA-Z0-9_]*
FPNUM	(\-?(([0-9]+)(\.[0-9]*)?)|(\.[0-9]+))

%% /* Begin rules */
<C_CODE>.	*str_next++ = yytext[0];
<C_CODE>\n	{
			*str_next++ = '\n';
			line_num++;
			if (one_line_c_code)
			{
				*str_next++ = 0;
				yylval.pchar = c_str;
				BEGIN SNL;
				RETURN(C_STMT);
			}
		}
<C_CODE>"}%"	{
			*str_next++ = 0;
			yylval.pchar = c_str;
			BEGIN SNL;
			RETURN(C_STMT);
		}
<COMMENT>\n	line_num++;
<COMMENT>"*/"	BEGIN SNL;
<COMMENT>.	;
<STR>"\\\""	{
			*str_next++ = yytext[0];
			*str_next++ = yytext[1];
		}
<STR>\"		{
			*str_next++ = 0;
			yylval.pchar = c_str;
			BEGIN SNL;
			RETURN(STRING);
		}
<STR>.		{ *str_next++ = yytext[0]; }
<STR>\n		{ *str_next++ = '?'; line_num++; }
<SNL>\n		{ line_num++; }
<SNL>"%{"	{
			c_str = str_next;
			one_line_c_code = FALSE;
			BEGIN C_CODE;
		}
<SNL>"%%"	{
			c_str = str_next;
			one_line_c_code = TRUE;
			BEGIN C_CODE;
		}
<SNL>"/*"	BEGIN COMMENT;
<SNL>\"		{ c_str = str_next; BEGIN STR; }
<SNL>"ss"	RETURN(STATE_SET);
<SNL>"state"	RETURN(STATE);
<SNL>"when"	RETURN(WHEN);
<SNL>"monitor"	RETURN(MONITOR);
<SNL>"assign"	RETURN(ASSIGN);
<SNL>"int"	RETURN(INT);
<SNL>"float"	RETURN(FLOAT);
<SNL>"double"	RETURN(DOUBLE);
<SNL>"short"	RETURN(SHORT);
<SNL>"char"	RETURN(CHAR);
<SNL>"string"	RETURN(STRING_DECL);
<SNL>"to"	RETURN(TO);
<SNL>"define"	RETURN(DEFINE);
<SNL>"program"	RETURN(PROGRAM);
<SNL>"debug"	RETURN(DEBUG_PRINT);
<SNL>"evflag"	RETURN(EVFLAG);
<SNL>"sync"	RETURN(SYNC);
<SNL>{NAME} {
		nc = strlen(yytext);
		bcopy(yytext, str_next, nc+1);
		yylval.pchar = str_next;
		str_next += nc+1;
		RETURN(NAME);
	}
<SNL>\=		RETURN(EQUAL);
<SNL>\:		RETURN(COLON);
<SNL>\&		RETURN(AMPERSAND);
<SNL>\*		RETURN(ASTERISK);
<SNL>\{		RETURN(L_BRACKET);
<SNL>\}		RETURN(R_BRACKET);
<SNL>\[		RETURN(L_SQ_BRACKET);
<SNL>\]		RETURN(R_SQ_BRACKET);
<SNL>\;		RETURN(SEMI_COLON);
<SNL>\.		RETURN(PERIOD);
<SNL>\(		RETURN(L_PAREN);
<SNL>\)		RETURN(R_PAREN);
<SNL>\,		RETURN(COMMA);
<SNL>"|"	RETURN(OR);
<SNL>"&"	RETURN(AND);
<SNL>"||"	RETURN(OR);
<SNL>"&&"	RETURN(AND);
<SNL>"+"	RETURN(PLUS);
<SNL>"-"	RETURN(MINUS);
<SNL>"/"	RETURN(SLASH);
<SNL>">"	RETURN(GT);
<SNL>">="	RETURN(GE);
<SNL>"=="	RETURN(EQ);
<SNL>"+="	RETURN(PLUS_EQUAL);
<SNL>"-="	RETURN(MINUS_EQUAL);
<SNL>"*="	RETURN(MULT_EQUAL);
<SNL>"/="	RETURN(DIV_EQUAL);
<SNL>"&="	RETURN(AND_EQUAL);
<SNL>"|="	RETURN(OR_EQUAL);
<SNL>"!="	RETURN(NE);
<SNL>"<="	RETURN(LE);
<SNL>"<"	RETURN(LT);
<SNL>"!"	RETURN(NOT);
<SNL>{FPNUM} {
		nc = strlen(yytext);
		bcopy(yytext, str_next, nc+1);
		yylval.pchar = str_next;
		str_next += nc+1;
		RETURN(NUMBER);
	}
<SNL>[\t\ ]*		;
<SNL>.		RETURN(BAD_CHAR);
.|\n		{ line_num = 1; BEGIN SNL; yyless(0); }

