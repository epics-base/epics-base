/*#define	DEBUG	1*/
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Parsing support routines for state notation compiler.
	 The 'yacc' parser calls these routines to build the tables
	 and linked lists, which are then passed on to the phase 2 routines.

	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Replaced lstLib calls with built-in links.
20nov91,ajk	Removed snc_init() - no longer did anything useful.
20nov91,ajk	Added option_stmt() routine.
***************************************************************************/

/*====================== Includes, globals, & defines ====================*/
#include	<ctype.h>
#include	<stdio.h>
#include	"parse.h"	/* defines linked list structures */
#include	<math.h>
#include	"db_access.h"

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif	TRUE

int	debug_print_flag = 0;	/* Debug level (set by source file) */

char	*prog_name;		/* ptr to program name (string) */

char	*prog_param;		/* parameter string for program stmt */

Expr	*defn_c_list;		/* definition C code list */

Expr	*ss_list;		/* Start of state set list */

Expr	*exit_code_list;	/* Start of exit code list */

Var	*var_list = NULL;	/* start of variable list */
Var	*var_tail = NULL;	/* tail of variable list */

Chan	*chan_list = NULL;	/* start of DB channel list */
Chan	*chan_tail = NULL;	/* tail of DB channel list */

Expr	*global_c_list;		/* global C code following state program */

/*+************************************************************************
*  NAME: program_name
*
*  CALLING SEQUENCE: none
*	type		argument	I/O	description
*	-----------------------------------------------------------------
*	char		*pname		I	ptr to program name string
*
*  RETURNS:
*
*  FUNCTION: Save program name for global use.
*
*-*************************************************************************/
program_name(pname, pparam)
char	*pname, *pparam;
{
	extern	char *prog_name, *prog_param;

	prog_name = pname;
	prog_param = pparam;
#ifdef	DEBUG
	fprintf(stderr, "program name = %s\n", prog_name);
#endif
	return;
}

/* Parsing a declaration statement */
decl_stmt(type, name, s_length, value)
int	type;		/* variable type (e.g. V_FLOAT) */
char	*name;		/* ptr to variable name */
char	*s_length;	/* array lth (NULL=single element) */
char	*value;		/* initial value or NULL */
{
	Var		*vp;
	int		length;
	extern int	line_num;

	if (s_length == 0)
		length = 0;
	else if (type == V_STRING)
		length = 0;
	else
	{
		length = atoi(s_length);
		if (length < 0)
			length = 0;
	}
#ifdef	DEBUG
	fprintf(stderr, "variable decl: type=%d, name=%s, length=%d\n",
	 type, name, length);
#endif
	/* See if variable already declared */
	vp = (Var *)findVar(name);
	if (vp != 0)
	{
		fprintf(stderr, "variable %s already declared, line %d\n",
		 name, line_num);
		return;
	}
	/* Build a struct for this variable */
	vp = allocVar();
	addVar(vp); /* add to var list */
	vp->name = name;
	vp->type = type;
	vp->length = length;
	vp->value = value; /* initial value or NULL */
	return;
}

/* Option statement */
option_stmt(option, value)
char		*option; /* "a", "r", ... */
int		value;	/* TRUE means +, FALSE means - */
{
	extern int	async_flag, conn_flag, debug_flag,
			line_flag, reent_flag, warn_flag;

	switch(*option)
	{
	    case 'a':
		async_flag = value;
		break;
	    case 'c':
		conn_flag = value;
		break;
	    case 'd':
		debug_flag = value;
		break;
	    case 'l':
		line_flag = value;
		break;
	    case 'r':
		reent_flag = value;
		break;
	    case 'w':
		warn_flag = value;
		break;
	}
	return;
}

/* "Assign" statement: assign a variable to a DB channel.
	elem_num is ignored in this version) */
assign_stmt(name, db_name)
char	*name;		/* ptr to variable name */
char	*db_name;	/* ptr to db name */
{
	Chan		*cp;
	Var		*vp;
	extern int	line_num;

#ifdef	DEBUG
	fprintf(stderr, "assign_stmt: name=%s, db_name=%s\n", name, db_name);
#endif	DEBUG
	/* Find the variable */
	vp = (Var *)findVar(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, line_num);
		return;
	}

	/* Build structure for this channel */
	cp = allocChan();
	addChan(cp);		/* add to Chan list */
	cp->var = vp;		/* make connection to variable */
	vp->chan = cp;		/* reverse ptr */
	cp->db_name = db_name;	/* DB name */
	cp->count = vp->length;	/* default count is variable length */

	cp->mon_flag = FALSE;	/* assume no monitor */
	cp->ef_var = NULL;	/* assume no sync event flag */
	return;
}

/* Parsing a "monitor" statement */
monitor_stmt(name, delta)
char	*name;		/* variable name (should be assigned) */
char	*delta;		/* monitor delta */
{
	Chan	*cp;
	extern int	line_num;

	/* Find a channel assigned to this variable */
	cp = (Chan *)findChan(name);
	if (cp == 0)
	{
		fprintf(stderr, "monitor: variable %s not assigned, line %d\n",
		 name, line_num);
		return;
	}

	/* Enter monitor parameters */
	cp->mon_flag = TRUE;
	cp->delta = atof(delta);
	return;
}
	
/* Parsing "sync" statement */
sync_stmt(name, ef_name)
char	*name;
char	*ef_name;
{
	Chan		*cp;
	Var		*vp;
	extern int	line_num;

	cp = (Chan *)findChan(name);
	if (cp == 0)
	{
		fprintf(stderr, "sync: variable %s not assigned, line %d\n",
		 name, line_num);
		return;
	}

	/* Find the event flag varible */
	vp = (Var *)findVar(ef_name);
	if (vp == 0 || vp->type != V_EVFLAG)
	{
		fprintf(stderr, "sync: e-f variable %s not declared, line %d\n",
		 ef_name, line_num);
		return;
	}

	/* Insert pointers between Var & Chan structures */
	cp->ef_var = vp;
	vp->chan = cp;

	return;
}
	
/* Definition C code */
defn_c_stmt(c_str)
char		*c_str; /* ptr to C code string */
{
	Expr		*ep;

#ifdef	DEBUG
	fprintf(stderr, "defn_c_stmt\n");
#endif
	ep = expression(E_TEXT, "", c_str, 0);
	if (defn_c_list == 0)
		defn_c_list = ep;
	else
		link_expr(defn_c_list, ep);	
	
	return;
}

/* Global C code (follows state program) */
global_c_stmt(c_str)
char		*c_str; /* ptr to C code */
{
	global_c_list = expression(E_TEXT, "", c_str, 0);

	return;
}


/* Add a variable to the variable linked list */
addVar(vp)
Var		*vp;
{
	if (var_list == NULL)
		var_list = vp;
	else
		var_tail->next = vp;
	var_tail = vp;
	vp->next = NULL;
}
	
/* Find a variable by name;  returns a pointer to the Var struct;
	returns 0 if the variable is not found. */
Var *findVar(name)
char		*name;
{
	Var		*vp;

#ifdef	DEBUG
	fprintf(stderr, "findVar, name=%s: ", name);
#endif
	for (vp = var_list; vp != NULL; vp = vp->next)
	{
		if (strcmp(vp->name, name) == 0)
		{
#ifdef	DEBUG
			fprintf(stderr, "found\n");
#endif
			return vp;
		}
	}
#ifdef	DEBUG
	fprintf(stderr, "not found\n");
#endif
	return 0;
}

/* Add a channel to the channel linked list */
addChan(cp)
Chan		*cp;
{
	if (chan_list == NULL)
		chan_list = cp;
	else
		chan_tail->next = cp;
	chan_tail = cp;
	cp->next = NULL;
}
	
/* Find a channel with a given associated variable name */
Chan *findChan(name)
char		*name;	/* variable name */
{
	Chan		*cp;
	Var		*vp;

#ifdef	DEBUG
	fprintf(stderr, "findChan, var name=%s: ", name);
#endif
	for (cp = chan_list; cp != NULL; cp = cp->next)
	{
		vp = cp->var;
		if (vp == 0)
			continue;
		if (strcmp(vp->name, name) == 0)
		{
#ifdef	DEBUG
			fprintf(stderr, "found chan name=%s\n", cp->db_name);
#endif
			return cp;
		}
	}
#ifdef	DEBUG
	fprintf(stderr, "not found\n");
#endif
	return 0;
}

/* Set debug print flag */
set_debug_print(flag)
char	*flag;
{
	debug_print_flag = atoi(flag);
}

/* Parsing "program" statement */
program(prog_list)
Expr		*prog_list;
{
	ss_list = prog_list;
#ifdef	DEBUG
	fprintf(stderr, "----Phase2---\n");
#endif	DEBUG
	phase2(ss_list);

	exit(0);
}

/* Exit code */
exit_code(ep)
Expr		*ep;
{
	exit_code_list = ep;
	return 0;
}

/* Build an expression list (hierarchical):
	Builds a node on a binary tree for each expression primitive.
 */
Expr *expression(type, value, left, right)
int		type;		/* E_BINOP, E_ASGNOP, etc */
char		*value;		/* "==", "+=", var name, contstant, etc. */	
Expr		*left;		/* LH side */
Expr		*right;		/* RH side */
{
	Expr		*ep;
	extern int	line_num, c_line_num;
	extern char	*src_file;
#ifdef	DEBUG
	extern char	*stype[];
#endif
	/* Allocate a structure for this item or expression */
	ep = allocExpr();
#ifdef	DEBUG
	fprintf(stderr,
	 "expression: ep=%d, type=%s, value=\"%s\", left=%d, right=%d\n",
	 ep, stype[type], value, left, right);
#endif
	/* Fill in the structure */
	ep->next = (Expr *)0;
	ep->last = ep;
	ep->type = type;
	ep->value = value;
	ep->left = left;
	ep->right = right;
	if (type == E_TEXT)
		ep->line_num = c_line_num;
	else
		ep->line_num = line_num;
	ep->src_file = src_file;

	return ep;
}

/* Link two expression structures and/or lists.  Returns ptr to combined list.
   Note:  ->last ptrs are correct only for 1-st and last structures in the list */
Expr *link_expr(ep1, ep2)
Expr		*ep1;	/* beginning of 1-st structure or list */
Expr		*ep2;	/* beginning 2-nd (append it to 1-st) */
{
	Expr		*ep;

	if (ep1 == 0)
		return ep2; /* shouldn't happen */
	else if (ep2 == 0)
		return ep1;

	(ep1->last)->next = ep2;
	ep1->last = ep2->last;
#ifdef	DEBUG
	fprintf(stderr, "link_expr(");
	for (ep = ep1; ; ep = ep->next)
	{
		fprintf(stderr, "%d, ", ep);
		if (ep == ep1->last)
			break;
	}
	fprintf(stderr, ")\n");	
#endif	DEBUG
	return ep1;
}

/* Interpret pre-processor code */
pp_code(line, fname)
char		*line;
char		*fname;
{
	extern int		line_num;

	line_num = atoi(line);
	src_file = fname;
}

/* The ordering of this list must correspond with the ordering in parse.h */
char	*stype[] = {
	"E_EMPTY", "E_CONST", "E_VAR", "E_FUNC", "E_STRING", "E_UNOP", "E_BINOP",
	"E_ASGNOP", "E_PAREN", "E_SUBSCR", "E_TEXT", "E_STMT", "E_CMPND",
	"E_IF", "E_ELSE", "E_WHILE", "E_SS", "E_STATE", "E_WHEN" };
