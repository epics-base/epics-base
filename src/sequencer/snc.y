%{
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	ENVIRONMENT: UNIX
	HISTORY:
20nov91,ajk	Added new "option" statement.
***************************************************************************/
/*	SNC - State Notation Compiler.
 *	The general structure of a state program is:
 *		program-name
 *		declarations
 *		ss  { state { event { action ...} new-state } ... } ...
 *
 *	The following yacc definitions call the various parsing routines, which
 *	are coded in the file "parse.c".  Their major action is to build
 *	a structure for each SNL component (state, event, action, etc.) and
 *	build linked lists from these structures.  The linked lists have a
 *	hierarchical structure corresponding to the SNL block structure.
 *	For instance, a "state" structure is linked to a chain of "event",
 *	structures, which are, in turn, linked to a chain of "action"
 *	structures.
 *	The lexical analyser (see snc_lax.l) reads the input
 *	stream and passes tokens to the yacc-generated code.  The snc_lex
 *	and parsing routines may also pass data elements that take on one
 *	of the types defined under the %union definition.
 *
 */
#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	"parse.h"

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif	/* TRUE */

extern	int line_num; /* input file line no. */
%}

%start	state_program

%union
{
	int	ival;
	char	*pchar;
	void	*pval;
	Expr	*pexpr;
}
%token	<pchar>	STATE STATE_SET
%token	<pchar>	NUMBER NAME
%token	<pchar>	DEBUG_PRINT
%token	PROGRAM EXIT OPTION
%token	R_SQ_BRACKET L_SQ_BRACKET
%token	BAD_CHAR L_BRACKET R_BRACKET
%token	COLON SEMI_COLON EQUAL
%token	L_PAREN R_PAREN PERIOD POINTER COMMA OR AND
%token	MONITOR ASSIGN TO WHEN CHAR SHORT INT LONG FLOAT DOUBLE STRING_DECL
%token	EVFLAG SYNC
%token	ASTERISK AMPERSAND
%token	AUTO_INCR AUTO_DECR
%token	PLUS MINUS SLASH GT GE EQ LE LT NE NOT BIT_OR BIT_AND
%token	L_SHIFT R_SHIFT COMPLEMENT MODULO
%token	PLUS_EQUAL MINUS_EQUAL MULT_EQUAL DIV_EQUAL AND_EQUAL OR_EQUAL
%token	MODULO_EQUAL LEFT_EQUAL RIGHT_EQUAL CMPL_EQUAL
%token	<pchar>	STRING
%token	<pchar>	C_STMT
%token	IF ELSE WHILE FOR BREAK
%token	PP_SYMBOL CR
%type	<ival>	type
%type	<pchar>	subscript binop asgnop unop
%type	<pexpr> state_set_list state_set state_list state transition_list transition
%type	<pexpr> parameter expr
%type	<pexpr> statement stmt_list compound_stmt if_stmt else_stmt while_stmt
%type	<pexpr> for_stmt
/* precidence rules for expr evaluation */
%left	OR AND
%left	GT GE EQ NE LE LT
%left	PLUS MINUS
%left	ASTERISK SLASH
%left	NOT UOP	/* unary operators: e.g. -x */
%left	SUBSCRIPT

%%	/* Begin rules */

state_program 	/* define a state program */
:	program_name definitions state_set_list			{ program($3); }
|	program_name definitions state_set_list global_c	{ program($3); }
|	pp_code program_name definitions state_set_list		{ program($4); }
|	pp_code program_name definitions state_set_list global_c{ program($4); }
|	error { snc_err("state program"); }
;

program_name 	/* program name */
:	PROGRAM NAME { program_name($2, ""); }
|	PROGRAM NAME L_PAREN STRING R_PAREN { program_name($2, $4); }
;

definitions 	/* definitions block */
:	defn_stmt
|	definitions defn_stmt
;

defn_stmt	/* individual definitions for SNL (preceeds state sets) */
:	assign_stmt
|	monitor_stmt
|	decl_stmt
|	debug_stmt
|	sync_stmt
|	option_stmt
|	C_STMT			{ defn_c_stmt($1); }
|	pp_code
|	error { snc_err("definitions/declarations"); }
;

assign_stmt	/* 'assign <var name> to <db name>;' */
:	ASSIGN NAME TO STRING SEMI_COLON { assign_stmt($2, $4); }
;

monitor_stmt	/* variable to be monitored; delta is optional */
:	MONITOR NAME SEMI_COLON		{ monitor_stmt($2, "0"); }
|	MONITOR NAME COMMA NUMBER SEMI_COLON	{ monitor_stmt($2, $4); }
;

subscript	/* e.g. [10] */
:	/* empty */				{ $$ = 0; }
|	L_SQ_BRACKET NUMBER R_SQ_BRACKET	{ $$ = $2; }
;

debug_stmt
:	DEBUG_PRINT NUMBER SEMI_COLON		{ set_debug_print($2); }
;

decl_stmt	/* variable declarations (e.g. float x[20];) */
:	type NAME subscript SEMI_COLON			{ decl_stmt($1, $2, $3, (char *)0); }
|	type NAME subscript EQUAL NUMBER SEMI_COLON	{ decl_stmt($1, $2, $3, $5); }
|	type NAME subscript EQUAL STRING SEMI_COLON	{ decl_stmt($1, $2, $3, $5); }
;

type		/* types for variables defined in SNL */
:	CHAR		{ $$ = V_CHAR; }
|	SHORT		{ $$ = V_SHORT; }
|	INT		{ $$ = V_INT; }
|	LONG		{ $$ = V_LONG; }
|	FLOAT		{ $$ = V_FLOAT; }
|	DOUBLE		{ $$ = V_DOUBLE; }
|	STRING_DECL	{ $$ = V_STRING; }
|	EVFLAG		{ $$ = V_EVFLAG; }
;

sync_stmt	/* sync <variable> <event flag> */
:	SYNC NAME TO NAME SEMI_COLON	{ sync_stmt($2, $4); }
|	SYNC NAME NAME SEMI_COLON	{ sync_stmt($2, $3); /* archaic syntax */ }
;

option_stmt	/* option +/-<option>;  e.g. option +a; */
:	OPTION PLUS NAME SEMI_COLON	{ option_stmt($3, TRUE); }
|	OPTION MINUS NAME SEMI_COLON	{ option_stmt($3, FALSE); }
;

state_set_list 	/* a program body is one or more state sets */
:	state_set			{ $$ = $1; }
|	state_set_list state_set	{ $$ = link_expr($1, $2); }
;

state_set 	/* define a state set */
:	STATE_SET NAME L_BRACKET state_list R_BRACKET
				{ $$ = expression(E_SS, $2, $4, 0); }
|	EXIT L_BRACKET stmt_list R_BRACKET
				{ exit_code($3); $$ = 0; }

|	error { snc_err("state set"); }
;

state_list /* define a state set body (one or more states) */
:	state				{ $$ = $1; }
|	state_list state		{ $$ = link_expr($1, $2); }
|	error { snc_err("state set"); }
;

state	/* a block that defines a single state */
:	STATE NAME L_BRACKET transition_list R_BRACKET
				{ $$ = expression(E_STATE, $2, $4, 0); }
|	error { snc_err("state block"); }
;

transition_list	/* all transitions for one state */
:	transition			{ $$ = $1; }
|	transition_list transition	{ $$ = link_expr($1, $2); }
|	error { snc_err("transition"); }
;

transition	/* define a transition ("when" statment ) */
:	WHEN L_PAREN expr R_PAREN L_BRACKET stmt_list R_BRACKET STATE NAME
			{ $$ = expression(E_WHEN, $9, $3, $6); }
;

expr	/* general expr: e.g. (-b+2*a/(c+d)) != 0 || (func1(x,y) < 5.0) */
	/* Expr *expression(int type, char *value, Expr *left, Expr *right) */
:	expr binop expr 		{ $$ = expression(E_BINOP, $2, $1, $3); }
|	expr asgnop expr		{ $$ = expression(E_ASGNOP, $2, $1, $3); }
|	unop expr  %prec UOP		{ $$ = expression(E_UNOP, $1, $2, 0); }
|	AUTO_INCR expr  %prec UOP	{ $$ = expression(E_PRE, "++", $2, 0); }
|	AUTO_DECR expr  %prec UOP	{ $$ = expression(E_PRE, "--", $2, 0); }
|	expr AUTO_INCR  %prec UOP	{ $$ = expression(E_POST, "++", $1, 0); }
|	expr AUTO_DECR  %prec UOP	{ $$ = expression(E_POST, "--", $1, 0); }
|	NUMBER				{ $$ = expression(E_CONST, $1, 0, 0); }
|	STRING				{ $$ = expression(E_STRING, $1, 0, 0); }
|	NAME				{ $$ = expression(E_VAR, $1, 0, 0); }
|	NAME L_PAREN parameter R_PAREN	{ $$ = expression(E_FUNC, $1, $3, 0); }
|	EXIT L_PAREN parameter R_PAREN	{ $$ = expression(E_FUNC, "exit", $3, 0); }
|	L_PAREN expr R_PAREN		{ $$ = expression(E_PAREN, "", $2, 0); }
|	expr L_SQ_BRACKET expr R_SQ_BRACKET %prec SUBSCRIPT
					{ $$ = expression(E_SUBSCR, "", $1, $3); }
|	/* empty */			{ $$ = expression(E_EMPTY, "", 0, 0); }
;

unop	/* Unary operators */
:	PLUS		{ $$ = "+"; }
|	MINUS		{ $$ = "-"; }
|	ASTERISK	{ $$ = "*"; }
|	AMPERSAND	{ $$ = "&"; }
|	NOT		{ $$ = "!"; }
;

binop	/* Binary operators */
:	MINUS		{ $$ = "-"; }
|	PLUS		{ $$ = "+"; }
|	ASTERISK	{ $$ = "*"; }
|	SLASH 		{ $$ = "/"; }
|	GT		{ $$ = ">"; }
|	GE		{ $$ = ">="; }
|	EQ		{ $$ = "=="; }
|	NE		{ $$ = "!="; }
|	LE		{ $$ = "<="; }
|	LT		{ $$ = "<"; }
|	OR		{ $$ = "||"; }
|	AND		{ $$ = "&&"; }
|	L_SHIFT		{ $$ = "<<"; }
|	R_SHIFT		{ $$ = ">>"; }
|	BIT_OR		{ $$ = "|"; }
|	BIT_AND		{ $$ = "&"; }
|	COMPLEMENT	{ $$ = "^"; }
|	MODULO		{ $$ = "%"; }
|	COMMA		{ $$ = ","; }
|	PERIOD		{ $$ = "."; }	/* fudges structure elements */
|	POINTER		{ $$ = "->"; }	/* fudges ptr to structure elements */
;

asgnop	/* Assignment operators */
:	EQUAL		{ $$ = "="; }
|	PLUS_EQUAL	{ $$ = "+="; }
|	MINUS_EQUAL	{ $$ = "-="; }
|	AND_EQUAL	{ $$ = "&="; }
|	OR_EQUAL	{ $$ = "|="; }
|	DIV_EQUAL	{ $$ = "/="; }
|	MULT_EQUAL	{ $$ = "*="; }
|	MODULO_EQUAL	{ $$ = "%="; }
|	LEFT_EQUAL	{ $$ = "<<="; }
|	RIGHT_EQUAL	{ $$ = ">>="; }
|	CMPL_EQUAL	{ $$ = "^="; }
;

parameter	/* expr, expr, .... */
:	expr			{ $$ = $1; }
|	parameter COMMA expr	{ $$ = link_expr($1, $3); }
|	/* empty */		{ $$ = 0; }
;

compound_stmt		/* compound statement e.g. { ...; ...; ...; } */
:	L_BRACKET stmt_list R_BRACKET { $$ = expression(E_CMPND, "",$2, 0); }
|	error { snc_err("action statements"); }	
;

stmt_list
:	statement		{ $$ = $1; }
|	stmt_list statement	{ $$ = link_expr($1, $2); }
|	/* empty */		{ $$ = 0; }
;

statement
:	compound_stmt			{ $$ = $1; }
|	expr SEMI_COLON			{ $$ = expression(E_STMT, "", $1, 0); }
|	BREAK SEMI_COLON		{ $$ = expression(E_BREAK, "", 0, 0); }
|	if_stmt				{ $$ = $1; }
|	else_stmt			{ $$ = $1; }
|	while_stmt			{ $$ = $1; }
|	for_stmt			{ $$ = $1; }
|	C_STMT				{ $$ = expression(E_TEXT, "", $1, 0); }
|	pp_code				{ $$ = 0; }
|	error 				{ snc_err("action statement"); }
;

if_stmt
:	IF L_PAREN expr R_PAREN statement { $$ = expression(E_IF, "", $3, $5); }
;

else_stmt
:	ELSE statement			{ $$ = expression(E_ELSE, "", $2, 0); }
;

while_stmt
:	WHILE L_PAREN expr R_PAREN statement { $$ = expression(E_WHILE, "", $3, $5); }
;

for_stmt
:	FOR L_PAREN expr SEMI_COLON expr SEMI_COLON expr R_PAREN statement
	{ $$ = expression(E_FOR, "", expression(E_X, "", $3, $5),
				expression(E_X, "", $7, $9) ); }
;

pp_code		/* pre-processor code (e.g. # 1 "test.st") */
:	PP_SYMBOL NUMBER STRING CR		{ pp_code($2, $3, ""); }
|	PP_SYMBOL NUMBER STRING NUMBER CR	{ pp_code($2, $3, $4); }
;

global_c
:	C_STMT		{ global_c_stmt($1); }
;
%%
#include "snc_lex.c"
#include "snc_main.c"
