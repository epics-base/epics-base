/*#define	DEBUG	1*/
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

< 	parse.c,v 1.3 1995/10/19 16:30:16 wright Exp
	DESCRIPTION: Parsing support routines for state notation compiler.
	 The 'yacc' parser calls these routines to build the tables
	 and linked lists, which are then passed on to the phase 2 routines.

	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Replaced lstLib calls with built-in links.
20nov91,ajk	Removed snc_init() - no longer did anything useful.
20nov91,ajk	Added option_stmt() routine.
28apr92,ajk	Implemented new event flag mode.
29opc93,ajk	Implemented assignment of pv's to array elements.
29oct93,ajk	Implemented variable class (VC_SIMPLE, VC_ARRAY, & VC_POINTER).
29oct93,ajk	Added 'v' (vxWorks include) option.
17may94,ajk	Removed old event flag (-e) option.
***************************************************************************/

/*====================== Includes, globals, & defines ====================*/
#include	<ctype.h>
#include	<stdio.h>
#include	"parse.h"	/* defines linked list structures */
#include	<math.h>

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif	TRUE

int	debug_print_opt = 0;	/* Debug level (set by source file) */

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
	prog_name = pname;
	prog_param = pparam;
#ifdef	DEBUG
	fprintf(stderr, "program name = %s\n", prog_name);
#endif
	return;
}

/* Parsing a declaration statement */
decl_stmt(type, class, name, s_length1, s_length2, value)
int	type;		/* variable type (e.g. V_FLOAT) */
int	class;		/* variable class (e.g. VC_ARRAY) */
char	*name;		/* ptr to variable name */
char	*s_length1;	/* array lth (1st dim, arrays only) */
char	*s_length2;	/* array lth (2nd dim, [n]x[m] arrays only) */
char	*value;		/* initial value or NULL */
{
	Var		*vp;
	int		length1, length2;
	extern int	line_num;

#ifdef	DEBUG
	fprintf(stderr, 
	 "variable decl: type=%d, class=%d, name=%s, ", type, class, name);
#endif
	length1 = length2 = 1;

	if (s_length1 != NULL)
	{
		length1 = atoi(s_length1);
		if (length1 <= 0)
			length1 = 1;
	}

	if (s_length2 != NULL)
	{
		length2 = atoi(s_length2);
		if (length2 <= 0)
			length2 = 1;
	}

#ifdef	DEBUG
	fprintf(stderr, "length1=%d, length2=%d\n", length1, length2);
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
	vp->class = class;
	vp->type = type;
	vp->length1 = length1;
	vp->length2 = length2;
	vp->value = value; /* initial value or NULL */
	vp->chan = NULL;
	return;
}

/* Option statement */
option_stmt(option, value)
char		*option; /* "a", "r", ... */
int		value;	/* TRUE means +, FALSE means - */
{
	extern int	async_opt, conn_opt, debug_opt, line_opt, 
			reent_opt, warn_opt, vx_opt, newef_opt;

	switch(*option)
	{
	    case 'a':
		async_opt = value;
		break;
	    case 'c':
		conn_opt = value;
		break;
	    case 'd':
		debug_opt = value;
		break;
 		case 'e':
		newef_opt = value;
		break;

	    case 'l':
		line_opt = value;
		break;
	    case 'r':
		reent_opt = value;
		break;
	    case 'w':
		warn_opt = value;
		break;
	    case 'v':
		vx_opt = value;
		break;
	}
	return;
}

/* "Assign" statement: Assign a variable to a DB channel.
 * Format: assign <variable> to <string;
 * Note: Variable may be subscripted.
 */
assign_single(name, db_name)
char	*name;		/* ptr to variable name */
char	*db_name;	/* ptr to db name */
{
	Chan		*cp;
	Var		*vp;
	int		subNum;
	extern int	line_num;

#ifdef	DEBUG
	fprintf(stderr, "assign %s to \"%s\";\n", name, db_name);
#endif	DEBUG
	/* Find the variable */
	vp = (Var *)findVar(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, line_num);
		return;
	}

	cp = vp->chan;
	if (cp != NULL)
	{
		fprintf(stderr, "assign: %s already assigned, line %d\n",
		 name, line_num);
		return;
	}

	/* Build structure for this channel */
	cp = (Chan *)build_db_struct(vp);

	cp->db_name = db_name;	/* DB name */

	/* The entire variable is assigned */
	cp->count = vp->length1 * vp->length2;

	return;
}

/* "Assign" statement: assign an array element to a DB channel.
 * Format: assign <variable>[<subscr>] to <string>; */
assign_subscr(name, subscript, db_name)
char	*name;		/* ptr to variable name */
char	*subscript;	/* subscript value or NULL */
char	*db_name;	/* ptr to db name */
{
	Chan		*cp;
	Var		*vp;
	int		subNum;
	extern int	line_num;

#ifdef	DEBUG
	fprintf(stderr, "assign %s[%s] to \"%s\";\n", name, subscript, db_name);
#endif	DEBUG
	/* Find the variable */
	vp = (Var *)findVar(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, line_num);
		return;
	}

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		fprintf(stderr, "assign: variable %s not an array, line %d\n",
		 name, line_num);
		return;
	}

	cp = vp->chan;
	if (cp == NULL)
	{
		/* Build structure for this channel */
		cp = (Chan *)build_db_struct(vp);
	}
	else if (cp->db_name != NULL)
	{
		fprintf(stderr, "assign: array %s already assigned, line %d\n",
		 name, line_num);
		return;
	}

	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= vp->length1)
	{
		fprintf(stderr, 
		 "assign: subscript %s[%d] is out of range, line %d\n",
		 name, subNum, line_num);
		return;
	}

	if (cp->db_name_list == NULL)
		alloc_db_lists(cp, vp->length1); /* allocate lists */
	else if (cp->db_name_list[subNum] != NULL)
	{
		fprintf(stderr, "assign: %s[%d] already assigned, line %d\n",
		 name, subNum, line_num);
		return;
	}
	
	cp->db_name_list[subNum] = db_name;
	cp->count = vp->length2; /* could be a 2-dimensioned array */

	return;
}

/* Assign statement: assign an array to multiple DB channels.
 * Format: assign <variable> to { <string>, <string>, ... };
 * Assignments for double dimensioned arrays:
 * <var>[0][0] assigned to 1st db name,
 * <var>[1][0] assigned to 2nd db name, etc.
 * If db name list contains fewer names than the array dimension,
 * the remaining elements receive NULL assignments.
 */
assign_list(name, db_name_list)
char	*name;		/* ptr to variable name */
Expr	*db_name_list;	/* ptr to db name list */
{
	Chan		*cp;
	Var		*vp;
	int		elem_num;
	extern int	line_num;

#ifdef	DEBUG
	fprintf(stderr, "assign %s to {", name);
#endif	DEBUG
	/* Find the variable */
	vp = (Var *)findVar(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, line_num);
		return;
	}

	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2)
	{
		fprintf(stderr, "assign: variable %s is not an array, line %d\n",
		 name, line_num);
		return;
	}

	cp = vp->chan;
	if (cp != NULL)
	{
		fprintf(stderr, "assign: variable %s already assigned, line %d\n",
		 name, line_num);
		return;
	}

	/* Build a db structure for this variable */
	cp = (Chan *)build_db_struct(vp);

	/* Allocate lists */
	alloc_db_lists(cp, vp->length1); /* allocate lists */

	/* fill in the array of pv names */
	for (elem_num = 0; elem_num < vp->length1; elem_num++)
	{
		if (db_name_list == NULL)
			break; /* end of list */

#ifdef	DEBUG
		fprintf(stderr, "\"%s\", ", db_name_list->value);
#endif	DEBUG
		cp->db_name_list[elem_num] = db_name_list->value; /* DB name */
		cp->count = vp->length2;
 
		db_name_list = db_name_list->next;
	}
#ifdef	DEBUG
		fprintf(stderr, "};\n");
#endif	DEBUG

	return;
}

/* Build a db structure for this variable */
build_db_struct(vp)
Var		*vp;
{
	Chan		*cp;

	cp = allocChan();
	addChan(cp);		/* add to Chan list */

	/* make connections between Var & Chan structures */
	cp->var = vp;
	vp->chan = cp;

	/* Initialize the structure */
	cp->db_name_list = 0;
	cp->mon_flag_list = 0;
	cp->ef_var_list = 0;
	cp->ef_num_list = 0;
	cp->num_elem = 0;
	cp->mon_flag = 0;
	cp->ef_var = 0;
	cp->ef_num = 0;

	return (int)cp;
}

/* Allocate lists for assigning multiple pv's to a variable */
alloc_db_lists(cp, length)
Chan		*cp;
int		length;
{
	/* allocate an array of pv names */
	cp->db_name_list = (char **)calloc(sizeof(char **), length);

	/* allocate an array for monitor flags */
	cp->mon_flag_list = (int *)calloc(sizeof(int **), length);

	/* allocate an array for event flag var ptrs */
	cp->ef_var_list = (Var **)calloc(sizeof(Var **), length);

	/* allocate an array for event flag numbers */
	cp->ef_num_list = (int *)calloc(sizeof(int **), length);

	cp->num_elem = length;

}

/* Parsing a "monitor" statement.
 * Format:
 * 	monitor <var>; - monitor a single variable or all elements in an array.
 * 	monitor <var>[<m>]; - monitor m-th element of an array.
 */
monitor_stmt(name, subscript)
char		*name;		/* variable name (should be assigned) */
char		*subscript;	/* element number or NULL */
{
	Var		*vp;
	Chan		*cp;
	int		subNum;
	extern int	line_num;

#ifdef	DEBUG
	fprintf(stderr, "monitor_stmt: name=%s[%s]\n", name, subscript);
#endif	DEBUG

	/* Find the variable */
	vp = (Var *)findVar(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, line_num);
		return;
	}

	/* Find a channel assigned to this variable */
	cp = vp->chan;
	if (cp == 0)
	{
		fprintf(stderr, "monitor: variable %s not assigned, line %d\n",
		 name, line_num);
		return;
	}

	if (subscript == NULL)
	{
		if (cp->num_elem == 0)
		{	/* monitor one channel for this variable */
			cp->mon_flag = TRUE;
			return;
		}

		/* else monitor all channels in db list */
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{	/* 1 pv per element of the array */
			cp->mon_flag_list[subNum] = TRUE;
		}
		return;
	}

	/* subscript != NULL */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		fprintf(stderr, "monitor: subscript of %s out of range, line %d\n",
		 name, line_num);
		return;
	}

	if (cp->num_elem == 0 || cp->db_name_list[subNum] == NULL)
	{
		fprintf(stderr, "monitor: %s[%d] not assigned, line %d\n",
		 name, subNum, line_num);
		return;
	}

	cp->mon_flag_list[subNum] = TRUE;
	return;
}
	
/* Parsing "sync" statement */
sync_stmt(name, subscript, ef_name)
char		*name;
char		*subscript;
char		*ef_name;
{
	Chan		*cp;
	Var		*vp;
	extern int	line_num;
	int		subNum;

#ifdef	DEBUG
	fprintf(stderr, "sync_stmt: name=%s, subNum=%s, ef_name=%s\n",
	 name, subscript, ef_name);
#endif	DEBUG

	vp = (Var *)findVar(name);
	if (vp == 0)
	{
		fprintf(stderr, "sync: variable %s not declared, line %d\n",
		 name, line_num);
		return;
	}

	cp = vp->chan;
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

	if (subscript == NULL)
	{	/* no subscript */
		if (cp->db_name != NULL)
		{	/* 1 pv assigned to this variable */
			cp->ef_var = vp;
			return;
		}

		/* 1 pv per element in the array */
		for (subNum = 0; subNum < cp->num_elem; subNum++)
		{
			cp->ef_var_list[subNum] = vp;
		}
		return;
	}

	/* subscript != NULL */
	subNum = atoi(subscript);
	if (subNum < 0 || subNum >= cp->num_elem)
	{
		fprintf(stderr,
		 "sync: subscript %s[%d] out of range, line %d\n",
		 name, subNum, line_num);
		return;
	}
	cp->ef_var_list[subNum] = vp; /* sync to a specific element of the array */

	return;
}
	
/* Definition C code */
defn_c_stmt(c_list)
Expr		*c_list; /* ptr to C code */
{
#ifdef	DEBUG
	fprintf(stderr, "defn_c_stmt\n");
#endif
	if (defn_c_list == 0)
		defn_c_list = c_list;
	else
		link_expr(defn_c_list, c_list);	
	
	return;
}

/* Global C code (follows state program) */
global_c_stmt(c_list)
Expr		*c_list; /* ptr to C code */
{
	global_c_list = c_list;

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

	for (vp = var_list; vp != NULL; vp = vp->next)
	{
		if (strcmp(vp->name, name) == 0)
		{
			return vp;
		}
	}
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

/* Set debug print opt */
set_debug_print(opt)
char	*opt;
{
	debug_print_opt = atoi(opt);
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
	extern char		*src_file;

	line_num = atoi(line);
	src_file = fname;
}

/* The ordering of this list must correspond with the ordering in parse.h */
char	*stype[] = {
	"E_EMPTY", "E_CONST", "E_VAR", "E_FUNC", "E_STRING", "E_UNOP", "E_BINOP",
	"E_ASGNOP", "E_PAREN", "E_SUBSCR", "E_TEXT", "E_STMT", "E_CMPND",
	"E_IF", "E_ELSE", "E_WHILE", "E_SS", "E_STATE", "E_WHEN",
	"E_FOR", "E_X", "E_PRE", "E_POST", "E_BREAK", "E_COMMA",
	"E_?", "E_?", "E_?", "E_?", "E_?", "E_?", "E_?", "E_?", "E_?" };
