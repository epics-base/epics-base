%{
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)snc.y	1.1	10/16/90
	ENVIRONMENT: UNIX
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
#include	"parse.h"

Expr	*expression();
Expr	*parameter();

extern	int line_num; /* input file line no. */
%}

%start	state_program

%union
{
	int	ival;
	char	*pchar;
	void	*pval;
	Expr	*expr;
}
%token	<pchar>	STATE STATE_SET
%token	<pchar>	NUMBER NAME DBNAME
%token	<pchar>	DEBUG_PRINT
%token	PROGRAM DEFINE
%token	R_SQ_BRACKET L_SQ_BRACKET
%token	BAD_CHAR L_BRACKET R_BRACKET
%token	COLON SEMI_COLON EQUAL
%token	L_PAREN R_PAREN PERIOD COMMA OR AND
%token	MONITOR ASSIGN TO WHEN INT FLOAT DOUBLE SHORT CHAR STRING_DECL
%token	EVFLAG SYNC
%token	ASTERISK AMPERSAND
%token	PLUS MINUS SLASH GT GE EQ LE LT NE NOT
%token	PLUS_EQUAL MINUS_EQUAL MULT_EQUAL DIV_EQUAL AND_EQUAL OR_EQUAL
%token	<pchar>	STRING
%token	<pchar>	C_STMT
%type	<ival>	type
%type	<pchar>	subscript
%type	<expr> parameter function expression
/* precidence rules for expression evaluation */
%left	OR AND
%left	GT GE EQ NE LE LT
%left	PLUS MINUS
%left	ASTERISK SLASH
%left	NOT UOP	/* unary operators: e.g. -x */
%left	SUBSCRIPT

%%	/* Begin rules */

state_program 	/* define a state program */
:	program_name definitions program_body		{ program(""); }
|	program_name definitions program_body C_STMT	{ program($4); }
|	error { snc_err("state program", line_num, 12); }
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
|	C_STMT			{ defn_c_stmt($1); }
|	error { snc_err("definitions", line_num, 1); }
;

assign_stmt	/* 'assign <var name>[<lower lim>,<upper lim>] to <db name>;' */
		/* subscript range is optional */
:	ASSIGN NAME TO STRING SEMI_COLON	{ assign_stmt($2, (char *)0, (char *)0, $4); }
|	ASSIGN NAME L_SQ_BRACKET NUMBER R_SQ_BRACKET TO STRING SEMI_COLON
						{ assign_stmt($2, $4, (char *)0, $7); }
|	ASSIGN NAME L_SQ_BRACKET NUMBER COMMA NUMBER R_SQ_BRACKET TO STRING SEMI_COLON
						{ assign_stmt($2, $4, $6, $9); }
;

monitor_stmt	/* variable to be monitored; subscript & delta are optional */
:	MONITOR NAME subscript SEMI_COLON		{ monitor_stmt($2, $3, "0"); }
|	MONITOR NAME subscript  COMMA NUMBER SEMI_COLON	{ monitor_stmt($2, $3,  $5); }
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
:	FLOAT		{ $$ = V_FLOAT; }
|	DOUBLE		{ $$ = V_DOUBLE; }
|	INT		{ $$ = V_INT; }
|	SHORT		{ $$ = V_SHORT; }
|	CHAR		{ $$ = V_CHAR; }
|	STRING_DECL	{ $$ = V_STRING; }
|	EVFLAG		{ $$ = V_EVFLAG; }
;

sync_stmt	/* sync <variable> <event flag> */
:	SYNC NAME subscript NAME SEMI_COLON	{ sync_stmt($2, $3, $4); }
|	SYNC NAME subscript TO NAME SEMI_COLON	{ sync_stmt($2, $3, $5); }
;

program_body 	/* a program body is one or more state sets */
:	state_set
|	program_body state_set
;

state_set 	/* define a state set */
:	STATE_SET NAME L_BRACKET state_set_body R_BRACKET	{ state_set($2); }
|	error { snc_err("state set", line_num, 3); }
;

state_set_body /* define a state set body (one or more state blocks) */
:	state_block
|	state_set_body state_block
|	error { snc_err("state set body", line_num, 4); }
;

state_block	/* a block that defines a single state */
:	STATE NAME L_BRACKET trans_list R_BRACKET
			{ state_block($2); }
|	error { snc_err("state block", line_num, 11); }
;

trans_list	/* all transitions for one state */
:	transition
|	trans_list transition
|	error { snc_err("transition", line_num, 5); }
;

transition	/* define a transition (e.g. "when (abc(x) | def(y, z)) state 2;" ) */
:	WHEN L_PAREN expression R_PAREN L_BRACKET action R_BRACKET STATE NAME
			{ transition($9, $3); }
;

expression	/* general expression: e.g. (-b+2*a/(c+d)) != 0 || (func1(x,y) < 5.0) */
		/* Expr *expression(int type, char *value, Expr *left, Expr *right) */
:	NUMBER				{ $$ = expression(E_CONST, $1, (Expr *)0, (Expr *)0); }
|	STRING				{ $$ = expression(E_STRING, $1, (Expr *)0, (Expr *)0); }
|	NAME				{ $$ = expression(E_VAR, $1, (Expr *)0, (Expr *)0); }
|	function			{ $$ = $1; }
|	expression PLUS expression 	{ $$ = expression(E_BINOP, "+", $1, $3); }
|	expression MINUS expression 	{ $$ = expression(E_BINOP, "-", $1, $3); }
|	expression ASTERISK expression 	{ $$ = expression(E_BINOP, "*", $1, $3); }
|	expression SLASH expression 	{ $$ = expression(E_BINOP, "/", $1, $3); }
|	expression GT expression 	{ $$ = expression(E_BINOP, ">", $1, $3); }
|	expression GE expression 	{ $$ = expression(E_BINOP, ">=", $1, $3); }
|	expression EQ expression 	{ $$ = expression(E_BINOP, "==", $1, $3); }
|	expression NE expression 	{ $$ = expression(E_BINOP, "!=", $1, $3); }
|	expression LE expression 	{ $$ = expression(E_BINOP, "<=", $1, $3); }
|	expression LT expression 	{ $$ = expression(E_BINOP, "<", $1, $3); }
|	expression OR expression 	{ $$ = expression(E_BINOP, "||", $1, $3); }
|	expression AND expression 	{ $$ = expression(E_BINOP, "&&", $1, $3); }
|	expression EQUAL expression	{ $$ = expression(E_BINOP, "=", $1, $3); }
|	expression PLUS_EQUAL expression	{ $$ = expression(E_BINOP, "+=", $1, $3); }
|	expression MINUS_EQUAL expression	{ $$ = expression(E_BINOP, "-=", $1, $3); }
|	expression AND_EQUAL expression	{ $$ = expression(E_BINOP, "&=", $1, $3); }
|	expression OR_EQUAL expression	{ $$ = expression(E_BINOP, "|=", $1, $3); }
|	expression DIV_EQUAL expression	{ $$ = expression(E_BINOP, "/=", $1, $3); }
|	expression MULT_EQUAL expression	{ $$ = expression(E_BINOP, "*=", $1, $3); }
|	L_PAREN expression R_PAREN	{ $$ = expression(E_PAREN, "", (Expr *)0, $2); }
|	PLUS expression  %prec UOP	{ $$ = expression(E_UNOP, "+", (Expr *)0, $2); }
|	MINUS expression %prec UOP	{ $$ = expression(E_UNOP, "-", (Expr *)0, $2); }
|	NOT expression			{ $$ = expression(E_UNOP, "!", (Expr *)0, $2); }
|	ASTERISK expression %prec UOP	{ $$ = expression(E_UNOP, "*", (Expr *)0, $2); }
|	AMPERSAND expression %prec UOP	{ $$ = expression(E_UNOP, "&", (Expr *)0, $2); }
|	expression L_SQ_BRACKET expression R_SQ_BRACKET %prec SUBSCRIPT
					{ $$ = expression(E_SUBSCR, "", $1, $3); }
|	/* empty */			{ $$ = expression(E_EMPTY, ""); }
;

function	/* function */
:	NAME L_PAREN parameter R_PAREN	{ $$ = expression(E_FUNC, $1, (Expr *)0, $3); }
;

parameter	/* expr, expr, .... */
:	expression			{ $$ = parameter($1, 0); }
|	parameter COMMA expression	{ $$ = parameter($3, $1); }
|	/* empty */			{ $$ = 0; }
;

action 		/* action block for a single state */
:	/* Empty */
|	action_item
|	action action_item
|	error { snc_err("action", line_num, 8); }
;

action_item	/* define an action */
:	expression SEMI_COLON	{ action_stmt($1); }
|	C_STMT			{ action_c_stmt($1); }
|	error 			{ snc_err("action statement", line_num, 9); }
;

%%
