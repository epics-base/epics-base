%{
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	snc_lex.l,v 1.2 1995/06/27 15:26:09 wright Exp
	ENVIRONMENT: UNIX
	HISTORY:
20nov91,ajk	Added OPTION token.
15jan92,ajk	Fixed to allow "!" as a unary operator.
17Jul92,rcz	changed warn_flag to warn_opt
17Jul92,rcz	Ported ajk version from lanl
04apr93,ajk	Increased STR_BFR_SIZE from 30000 to 300000
19nov93,ajk	Added definitions for octal and hex numbers.
19nov93,ajk	Removed optional "-" from definition of FPNUM.  This was
		causing problems with statements like "k-1".
27man94,ajk	Implemented dynamic allocation of strings, thus eliminating
		huge string buffer.
31may94,ajk	Changed handling of escaped C code.
17feb95,ajk	Removed includes "parse.h" & "snc.h", because this module
		now gets included in snc.y.
20jul95,ajk	Added unsigned types.
11jul96,ajk	Added character constants
***************************************************************************/
/* Lexical analyzer for State Notation Compiler (snc).
 *
 * This routine recognizes State Notation Language (SNL) syntax,
 * and passes tokens to yacc().
 * All C code is passed through as a stream, without conversion.
 * Hence, the C compiler may find errors not reported by SNC.
 * Comments are recognized as part of the syntax.
 */

#include	"string.h"

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif
/*#define	DEBUG 1*/
#ifdef	DEBUG
#define	RETURN(param)	{ fprintf(stderr, "return(param)\n"); return(param); }
#else
#define	RETURN(param)	return(param);
#endif
#define	STR_BFR_SIZE	1000

extern int	line_num, c_line_num;	/* input line number */
extern int	warn_opt;		/* compiler warning flag */
char		strBfr[STR_BFR_SIZE];	/* holding place for strings */
char		*pStr;			/* current ptr to strBfr */
double		atof();
int		one_line_c_code;	/* TRUE for %%; FALSE for %{...}% */

%}

/* Start conditions (SNL, C code, comment, string, pre-processor, pre-proc. string) */
%Start	SNL C_CODE COMMENT STR PP PP_STR

NAME	[a-zA-Z][a-zA-Z0-9_]*
FPNUM	(([0-9]+)(\.[0-9]*)?)|(\.[0-9]+)
OCTAL	0[0-7]+
HEX	0x([0-9a-fA-F])+
CCONST	\'.\'

%% /* Begin rules */
<C_CODE>.	*pStr++ = yytext[0];
<C_CODE>\n {
		*pStr++ = 0;
		line_num++;
		c_line_num = line_num;
		yylval.pchar = strdup(strBfr);
		pStr = strBfr;
		if (one_line_c_code)
			BEGIN SNL;
		RETURN(C_STMT);
}
<C_CODE>"}%" {
		*pStr++ = 0;
		yylval.pchar = strdup(strBfr);
		BEGIN SNL;
		RETURN(C_STMT);
}
<COMMENT>\n	line_num++;
<COMMENT>"*/"	BEGIN SNL;
<COMMENT>.	;
<STR>"\\\"" {
		*pStr++ = yytext[0];
		*pStr++ = yytext[1];
}
<STR>\"	 {
		*pStr++ = 0;
		yylval.pchar = strdup(strBfr);
		BEGIN SNL;
		RETURN(STRING);
}
<STR>.		{ *pStr++ = yytext[0]; }
<STR>\n	{	*pStr++ = '?';
		if (warn_opt)
			fprintf(stderr, "Warning:  newline in string, line %d\n",
			 line_num);
		line_num++;
}
<PP>{FPNUM} {	yylval.pchar = strdup(yytext);
		RETURN(NUMBER);
}
<PP>\"		{ pStr = strBfr; BEGIN PP_STR; }
<PP>\n {
		BEGIN SNL;
		RETURN(CR);
}
<PP>[\t\ ]*	/* no action */ ;
<PP>. {
}
<PP_STR>\" {
		*pStr++ = 0;
		yylval.pchar = strdup(strBfr);
		BEGIN PP;
		RETURN(STRING);
}
<PP_STR>.	{ *pStr++ = yytext[0]; }
<PP_STR>\n	{ *pStr++ = '?'; line_num++; }
<SNL>\n		{ line_num++; }
<SNL>"%{" {
		pStr = strBfr;
		one_line_c_code = FALSE;
		c_line_num = line_num;
		BEGIN C_CODE;
}
<SNL>"%%" {
		pStr = strBfr;
		one_line_c_code = TRUE;
		c_line_num = line_num;
		BEGIN C_CODE;
}
<SNL>^# {
		BEGIN PP;
		RETURN(PP_SYMBOL);
}
<SNL>"/*"	BEGIN COMMENT;
<SNL>\"		{ pStr = strBfr; BEGIN STR; }
<SNL>"ss"	RETURN(STATE_SET);
<SNL>"state"	RETURN(STATE);
<SNL>"when"	RETURN(WHEN);
<SNL>"monitor"	RETURN(MONITOR);
<SNL>"assign"	RETURN(ASSIGN);
<SNL>"unsigned"	RETURN(UNSIGNED);
<SNL>"char"	RETURN(CHAR);
<SNL>"short"	RETURN(SHORT);
<SNL>"int"	RETURN(INT);
<SNL>"long"	RETURN(LONG);
<SNL>"float"	RETURN(FLOAT);
<SNL>"double"	RETURN(DOUBLE);
<SNL>"string"	RETURN(STRING_DECL);
<SNL>"to"	RETURN(TO);
<SNL>"program"	RETURN(PROGRAM);
<SNL>"option"	RETURN(OPTION);
<SNL>"debug"	RETURN(DEBUG_PRINT);
<SNL>"evflag"	RETURN(EVFLAG);
<SNL>"sync"	RETURN(SYNC);
<SNL>"if"	RETURN(IF);
<SNL>"else"	RETURN(ELSE);
<SNL>"while"	RETURN(WHILE);
<SNL>"for"	RETURN(FOR);
<SNL>"break"	RETURN(BREAK);
<SNL>"exit"	RETURN(EXIT);
<SNL>"TRUE" {	yylval.pchar = "1";
		RETURN(NUMBER);
}
<SNL>"FALSE" {
		yylval.pchar = "0";
		RETURN(NUMBER);
}
<SNL>{NAME} {
		yylval.pchar = strdup(yytext);
		RETURN(NAME);
}
<SNL>"++"	RETURN(AUTO_INCR);
<SNL>"--"	RETURN(AUTO_DECR);
<SNL>"||"	RETURN(OR);
<SNL>"<<="	RETURN(LEFT_EQUAL);
<SNL>">>="	RETURN(RIGHT_EQUAL);
<SNL>"&&"	RETURN(AND);
<SNL>">>"	RETURN(R_SHIFT);
<SNL>">="	RETURN(GE);
<SNL>"=="	RETURN(EQ);
<SNL>"+="	RETURN(PLUS_EQUAL);
<SNL>"-="	RETURN(MINUS_EQUAL);
<SNL>"*="	RETURN(MULT_EQUAL);
<SNL>"/="	RETURN(DIV_EQUAL);
<SNL>"&="	RETURN(AND_EQUAL);
<SNL>"|="	RETURN(OR_EQUAL);
<SNL>"!="	RETURN(NE);
<SNL>"<<"	RETURN(L_SHIFT);
<SNL>"<="	RETURN(LE);
<SNL>"%="	RETURN(MODULO_EQUAL);
<SNL>"^="	RETURN(CMPL_EQUAL);
<SNL>"->"	RETURN(POINTER);
<SNL>"+"	RETURN(PLUS);
<SNL>"-"	RETURN(MINUS);
<SNL>"!"	RETURN(NOT);
<SNL>"/"	RETURN(SLASH);
<SNL>"<"	RETURN(LT);
<SNL>">"	RETURN(GT);
<SNL>"|"	RETURN(BIT_OR);
<SNL>"&"	RETURN(BIT_AND);
<SNL>"^"	RETURN(COMPLEMENT);
<SNL>"%"	RETURN(MODULO);
<SNL>"="	RETURN(EQUAL);
<SNL>"&"	RETURN(AMPERSAND);
<SNL>"*"	RETURN(ASTERISK);
<SNL>"{"	RETURN(L_BRACKET);
<SNL>"}"	RETURN(R_BRACKET);
<SNL>"["	RETURN(L_SQ_BRACKET);
<SNL>"]"	RETURN(R_SQ_BRACKET);
<SNL>":"	RETURN(COLON);
<SNL>";"	RETURN(SEMI_COLON);
<SNL>"."	RETURN(PERIOD);
<SNL>"("	RETURN(L_PAREN);
<SNL>")"	RETURN(R_PAREN);
<SNL>","	RETURN(COMMA);
<SNL>{HEX} {
		yylval.pchar = strdup(yytext);
		RETURN(NUMBER);
}
<SNL>{OCTAL} {
		yylval.pchar = strdup(yytext);
		RETURN(NUMBER);
}
<SNL>{FPNUM} {
		yylval.pchar = strdup(yytext);
		RETURN(NUMBER);
}
<SNL>{CCONST} {
		yylval.pchar = strdup(yytext);
		RETURN(CHAR_CONST);
}
<SNL>[\t\ ]*	/* no action */ ;
<SNL>.		RETURN(BAD_CHAR);
# {		/* somehow "^#" doesn't work if it's first char */
		BEGIN PP;
		RETURN(PP_SYMBOL)
}
.|\n		{ line_num = 1; BEGIN SNL; yyless(0); }

