/* #define	DEBUG	1 */
/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)parse.c	1.1	10/16/90
	DESCRIPTION: Parsing support routines for state notation compiler.
	 The 'yacc' parser calls these routines to build the tables
	 and linked lists, which are then passed on to the phase 2 routines.

	ENVIRONMENT: UNIX
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

int	line_num;		/* Current line number */

char	*prog_name;		/* ptr to program name (string) */

char	*prog_param;		/* parameter string for program stmt */

LIST	defn_c_list;		/* definition C code */

LIST	ss_list;		/* start of state-set list */

LIST	var_list;		/* start of variable list */

LIST	chan_list;		/* start of DB channel list */

LIST	state_list;		/* temporary state list */

LIST	action_list;		/* temporary action list */

LIST	trans_list;		/* temporary transition list */

char	*global_c_code;		/* global C code following state program */
/*+************************************************************************
*  NAME: snc_init
*
*  CALLING SEQUENCE: none
*
*  RETURNS:
*
*  FUNCTION: Initialize state program tables & linked lists
*
*  NOTES:
*-*************************************************************************/
init_snc()
{
	lstInit(&defn_c_list);
	lstInit(&ss_list);
	lstInit(&var_list);
	lstInit(&chan_list);
	lstInit(&state_list);
	lstInit(&action_list);
	lstInit(&trans_list);
	return;
}
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
	printf("program name = %s\n", prog_name);
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
	Var	*vp;
	int	length;

	if (s_length == 0)
		length = 0;
	else if (type == V_STRING)
		length = MAX_STRING_SIZE;
	else
	{
		length = atoi(s_length);
		if (length < 0)
			length = 0;
	}
#ifdef	DEBUG
	printf("variable decl: type=%d, name=%s, length=%d\n", type, name, length);
#endif
	/* See if variable already declared */
	vp = (Var *)find_var(name);
	if (vp != 0)
	{
		fprintf(stderr, "variable %s already declared, line %d\n",
		 name, line_num);
		return;
	}
	/* Build a struct for this variable */
	vp = allocVar();
	lstAdd(&var_list, (NODE *)vp);
	vp->name = name;
	vp->type = type;
	vp->length = length;
	vp->value = value; /* initial value or NULL */
	return;
}

/* "Assign" statement: assign a variable to a DB channel.
	elem_num is ignored in this version) */
assign_stmt(name, lower_lim, upper_lim, db_name)
char	*name;		/* ptr to variable name */
char	*lower_lim;	/* ptr to lower array limit */
char	*upper_lim;	/* ptr to upper array limit */
char	*db_name;	/* ptr to db name */
{
	Chan	*cp;
	Var	*vp;
	int	range_low, range_high, length;

	/* Find the variable */
	vp = (Var *)find_var(name);
	if (vp == 0)
	{
		fprintf(stderr, "assign: variable %s not declared, line %d\n",
		 name, line_num);
		return;
	}

	/* Build structure for this channel */
	cp = allocChan();
	lstAdd(&chan_list, (NODE *)cp);
	cp->var = vp;		/* make connection to variable */
	vp->chan = cp;		/* reverse ptr */
	cp->db_name = db_name;	/* DB name */

	/* convert specified range REMOVED********* */
	length = vp->length;
	if (length == 0)
		length = 1; /* not an array */
	if (vp->type == V_STRING)
		length = 1; /* db treats strings as length 1 */
	cp->length = length;
	cp->offset = 0;
	cp->mon_flag = FALSE;	/* assume no monitor */
	cp->ef_var = NULL;	/* assume no sync event flag */
	return;
}
	
/* Build the range string, e.g. "0", and "9" into "0,9" */
char *
range(low, high)
char	*low, *high;
{
	static	char range_str[30];
	sprintf(range_str, "%s,%s", low, high);
	return range_str;
}


/* Parsing a "monitor" statement */
monitor_stmt(name, offset, delta)
char	*name;		/* variable name (should be assigned) */
char	*offset;	/* array offset */
char	*delta;		/* monitor delta */
{
	Chan	*cp;
	int	offs;

	/* Pick up array offset */
	if (offset == 0)
		offs = 0;
	else
		offs = atoi(offset);
	if (offs < 0)
		offs = 0;
	/* Find a channel assigned to this variable */
	cp = (Chan *)find_chan(name, offs);
	if (cp == 0)
	{
		fprintf(stderr, "monitor: variable %s[%s] not assigned, line %d\n",
		 name, offset, line_num);
		return;
	}

	/* Enter monitor parameters */
	cp->mon_flag = TRUE;
	cp->delta = atof(delta);
	return;
}
	
/* Parsing "sync" statement */
sync_stmt(name, offset, ef_name)
char	*name;
char	*offset;
char	*ef_name;
{
	Chan	*cp;
	Var	*vp;
	int	offs;

	/* Find the variable */
	if (offset == 0)
		offs = 0;
	else
		offs = atoi(offset);
	if (offs < 0)
		offs = 0;
	cp = (Chan *)find_chan(name, offs);
	if (cp == 0)
	{
		fprintf(stderr, "sync: variable %s[%s] not assigned, line %d\n",
		 name, offset, line_num);
		return;
	}
	/* Find the event flag varible */
	vp = (Var *)find_var(ef_name);
	if (vp == 0 || vp->type != V_EVFLAG)
	{
		fprintf(stderr, "sync: e-f variable %s not declared, line %d\n",
		 ef_name, line_num);
		return;
	}
	cp->ef_var = vp;

	return;
}
	
/* Definition C code */
defn_c_stmt(c_str)
char	*c_str;
{
	Action	*ap;
#ifdef	DEBUG
	printf("defn_c_stmt\n");
#endif
	ap = allocAction();
	lstAdd(&defn_c_list, (NODE *)ap);
	ap->type = A_CCODE;
	ap->stmt.c_code = c_str;
	ap->line_num = line_num;
	
	return;
}

/* Parsing "ss" statement */
state_set(ss_name)
char	*ss_name;
{
	StateSet	*ssp;

# ifdef	DEBUG
	printf("state_set: ss_name=%s\n", ss_name);
#endif
	/* Is this state set already declared ? */
	if (find_state_set(ss_name) != NULL)
	{
		fprintf(stderr, "Duplicate state set name: %s, line %d\n",
		 ss_name, line_num);
		return;
	}

	/* create and fill in state-set block */
	ssp = allocSS();
	lstAdd(&ss_list, (NODE *)ssp);
	ssp->ss_name = ss_name;
	/* move temporary state list into this node */
#ifdef	DEBUG
	printf("  # states = %d\n", lstCount(&state_list));
#endif	DEBUG
	ssp->S_list = state_list;
	lstInit(&state_list);

	return;
}


state_block(state_name)
char	*state_name;
{
	State	*sp;

#ifdef	DEBUG
	printf("state_block: state_name=%s\n", state_name);
#endif
	sp = firstState(&state_list);
	if (find_state(state_name, sp) != NULL)
	{
		fprintf(stderr, "Duplicate state name: %s, line %d\n",
		 state_name, line_num);
		return;
	}

	sp = allocState();
	lstAdd(&state_list, (NODE *)sp);
	sp->state_name = state_name;
	/* move tmp transition list into this node */
	sp->T_list = trans_list;
	lstInit(&trans_list);
	return;
}

/* Parsing transition part of "when" clause */
transition(new_state_name, event_expr)
char	*new_state_name;
Expr	*event_expr;	/* event expression */
{
	Trans	*tp;

#ifdef	DEBUG
	printf("transition: new_state_name=%s\n", new_state_name);
#endif
	tp = allocTrans();
	lstAdd(&trans_list, (NODE *)tp);
	tp->new_state_name = new_state_name;
	tp->line_num = line_num;
	/* move tmp action list into this node */
	tp->A_list = action_list;
	tp->event_expr = event_expr;
	lstInit(&action_list);

	return;
}

/* Action statement (lvalue = expression;) or (function())*/
action_stmt(ep)
Expr	*ep; 	/* ptr to expression */
{
	Action	*ap;
#ifdef	DEBUG
	printf("action_function\n");
#endif
	ap = allocAction();
	lstAdd(&action_list, (NODE *)ap);
	ap->type = A_STMT;
	ap->stmt.expr = ep;
	ap->line_num = line_num;

	return;
}

/* Action statement: in-line C code */
action_c_stmt(c_str)
char	*c_str;
{
	Action	*ap;
#ifdef	DEBUG
	printf("action_c_stmt\n");
#endif
	ap = allocAction();
	lstAdd(&action_list, (NODE *)ap);
	ap->type = A_CCODE;
	ap->stmt.c_code = c_str;
	ap->line_num = line_num;
	
	return;
}


/* Find a variable by name;  returns a pointer to the Var struct;
	returns 0 if the variable is not found. */
find_var(name)
char	*name;
{
	Var	*vp;

#ifdef	DEBUG
	printf("find_var, name=%s: ", name);
#endif
	vp = firstVar(&var_list);
	while (vp != NULL)
	{
		if (strcmp(vp->name, name) == 0)
		{
#ifdef	DEBUG
			printf("found\n");
#endif
			return (int)vp;
		}
		vp = nextVar(vp);
	}
#ifdef	DEBUG
	printf("not found\n");
#endif
	return 0;
}

/* Find channel with same variable name and same offset */
find_chan(name, offset)
char	*name;	/* variable name */
int	offset;/* array offset */
{
	Chan	*cp;
	Var	*vp;
	int	chan_offs;

	if (offset < 0)
		offset = 0;
	for (cp = firstChan(&chan_list); cp != NULL; cp = nextChan(cp))
	{
		vp = cp->var;
		if (vp == 0)
			continue;
		chan_offs = cp->offset;
		if (chan_offs < 0)
			chan_offs = 0;
		if (strcmp(vp->name, name) == 0 && chan_offs == offset)
		{
			return (int)cp;
		}
	}
	return 0;
}

/* Given the name of a state, find ptr to the state struct */
find_state(state_name, sp)
char	*state_name;	/* state name we're looking for */
State	*sp;		/* start of state list */
{
	for (; sp != NULL; sp = nextState(sp))
	{
		if (strcmp(sp->state_name, state_name) == 0)
			return (int)sp;
	}
	return NULL;	/* not found */
}

/* Given the name of a state set, find ptr to state set struc */
find_state_set(ss_name)
char	*ss_name;	/* state set name we're looking for */
{
	StateSet *ssp;
	extern	LIST ss_list;

	for (ssp = firstSS(&ss_list); ssp != NULL; ssp = nextSS(ssp))
	{
		if (strcmp(ssp->ss_name, ss_name) == 0)
			return (int)ssp;
	}
	return NULL;	/* not found */
}

/* Set debug print flag */
set_debug_print(flag)
char	*flag;
{
	debug_print_flag = atoi(flag);
}

/* Parsing "program" statement */
program(c_code)
char	*c_code;
{
	global_c_code = c_code;
	phase2();

	exit(0);
}

/* Build an expression list (hierarchical) */
Expr *expression(type, value, left, right)
int	type;
char	*value;
Expr	*left, *right;
{
	Expr	*ep;

	/* alloc a block for this item or expression */
#ifdef	DEBUG
	fprintf(stderr, "expression: type=%d, value=%s, left=%d, right=%d\n",
	 type, value, left, right);
#endif
	ep = allocExpr();
	ep->type = type;
	ep->value.const = value;
	ep->left = left;
	ep->right = right;
	ep->line_num = line_num;

	switch (type)
	{
	case E_VAR: /* variable or pre-defined constant */
		if (pre_defined_const(ep))
		{
			ep->type = E_CONST;
			break; /* defined constant */
		}
		/* find the variable */
		ep->value.var = (Var *)find_var(value);
		if (ep->value.var == 0)
		{
			fprintf(stderr, "variable \"%s\" not declared, line no. %d\n",
			 value, ep->line_num);
		}
		break;

	case E_CONST: /* number const */
		break;

	case E_STRING: /* string const */
		break;

	case E_BINOP: /* binary op. ( left_expr OP rt_expr ) */
		break;

	case E_UNOP: /* unary op. */
		break;

	case E_PAREN: /* parenthetical expr. */
		break;

	case E_FUNC:
		break;

	case E_EMPTY: /* empty */
		break;

	case E_SUBSCR: /* subscript */
		break;

	default: /* any other */
		fprintf(stderr, "? %d: %s, line no. %d\n", type, value, line_num);
		break;
	}

	return ep;
}

/* Check for defined constant */
pre_defined_const(ep)
Expr	*ep;
{
	if ( (strcmp(ep->value.const, "TRUE") == 0)
	  || (strcmp(ep->value.const, "FALSE") == 0) )
		return TRUE;
	else
		return FALSE;
}		

/* Add a parameter (expression) to end of list */
Expr *parameter(ep, p_list)
Expr	*ep;
Expr	*p_list; /* current list */
{
	Expr	*ep1;

	ep->next = 0;
	if (p_list == 0)
		return ep;

	ep1 = p_list;
	while (ep1->next != 0)
		ep1 = ep1->next;
	ep1->next = ep;
	return p_list;
}

/*#define	PHASE2*/
#ifdef	PHASE2
phase2()
{
	fprintf(stderr, "phase2() - dummy\07\n");
}
#endif	PHASE2
