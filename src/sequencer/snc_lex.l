%{
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	ENVIRONMENT: UNIX
	HISTORY:
20nov91,ajk	Added OPTION token.
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
#include	"stdio.h"
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
#define	STR_BFR_SIZE	30000

extern int	line_num, c_line_num; /* input line number */
extern int	warn_flag; /* compiler warning flag */
char		str_bfr[STR_BFR_SIZE]; /* holding place for strings */
char		*str_next = str_bfr; /* current ptr to str_bfr */
char		*c_str; /* Start of current string in str_bfr */
int		nc;
double		atof();
int		one_line_c_code;

%}

/* Start conditions (SNL, C code, comment, string, pre-processor, pre-proc. string) */
%Start	SNL C_CODE COMMENT STR PP PP_STR

NAME	[a-zA-Z][a-zA-Z0-9_]*
FPNUM	(\-?(([0-9]+)(\.[0-9]*)?)|(\.[0-9]+))

%% /* Begin rules */
<C_CODE>.	*str_next++ = yytext[0];
<C_CODE>\n {
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
<C_CODE>"}%" {
		*str_next++ = 0;
		yylval.pchar = c_str;
		BEGIN SNL;
		RETURN(C_STMT);
}
<COMMENT>\n	line_num++;
<COMMENT>"*/"	BEGIN SNL;
<COMMENT>.	;
<STR>"\\\"" {
		*str_next++ = yytext[0];
		*str_next++ = yytext[1];
}
<STR>\"	 {
		*str_next++ = 0;
		yylval.pchar = c_str;
		BEGIN SNL;
		RETURN(STRING);
}
<STR>.		{ *str_next++ = yytext[0]; }
<STR>\n	{	*str_next++ = '?';
		if (warn_flag)
			fprintf(stderr, "Warning:  newline in string, line %d\n",
			 line_num);
		line_num++;
}
<PP>{FPNUM} {	nc = strlen(yytext);
		bcopy(yytext, str_next, nc+1);
		yylval.pchar = str_next;
		str_next += nc+1;
		RETURN(NUMBER);
}
<PP>\"		{ c_str = str_next; BEGIN PP_STR; }
<PP>\n {
		BEGIN SNL;
		RETURN(CR);
}
<PP>[\t\ ]*	/* no action */ ;
<PP>. {
}
<PP_STR>\" {
		*str_next++ = 0;
		yylval.pchar = c_str;
		BEGIN PP;
		RETURN(STRING);
}
<PP_STR>.	{ *str_next++ = yytext[0]; }
<PP_STR>\n	{ *str_next++ = '?'; line_num++; }
<SNL>\n		{ line_num++; }
<SNL>"%{" {
		c_str = str_next;
		one_line_c_code = FALSE;
		c_line_num = line_num;
		BEGIN C_CODE;
}
<SNL>"%%" {
		c_str = str_next;
		one_line_c_code = TRUE;
		c_line_num = line_num;
		BEGIN C_CODE;
}
<SNL>^# {
		BEGIN PP;
		RETURN(PP_SYMBOL);
}
<SNL>"/*"	BEGIN COMMENT;
<SNL>\"		{ c_str = str_next; BEGIN STR; }
<SNL>"ss"	RETURN(STATE_SET);
<SNL>"state"	RETURN(STATE);
<SNL>"when"	RETURN(WHEN);
<SNL>"monitor"	RETURN(MONITOR);
<SNL>"assign"	RETURN(ASSIGN);
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
		nc = strlen(yytext);
		bcopy(yytext, str_next, nc+1);
		yylval.pchar = str_next;
		str_next += nc+1;
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
<SNL>"+"	RETURN(PLUS);
<SNL>"-"	RETURN(MINUS);
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
<SNL>{FPNUM} {
		nc = strlen(yytext);
		bcopy(yytext, str_next, nc+1);
		yylval.pchar = str_next;
		str_next += nc+1;
		RETURN(NUMBER);
}
<SNL>[\t\ ]*	/* no action */ ;
<SNL>.		RETURN(BAD_CHAR);
# {		/* somehow "^#" doesn't work if it's first char */
		BEGIN PP;
		RETURN(PP_SYMBOL)
}
.|\n		{ line_num = 1; BEGIN SNL; yyless(0); }

