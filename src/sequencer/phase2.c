/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Phase 2 code generation routines for SNC.
	Produces code and tables in C output file.
	See also:  gen_ss_code.c
	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Replaced lstLib calls with built-in linked list.
19nov91,ajk	Removed extraneous "static" from "UserVar" declaration.
***************************************************************************/

#include	<stdio.h>
#include	"parse.h"
#include <dbDefs.h>
#include <seqU.h>

int	num_channels = 0;	/* number of db channels */
int	num_ss = 0;		/* number of state sets */
int	num_errors = 0;		/* number of errors detected in phase 2 processing */

/*+************************************************************************
*  NAME: phase2
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*
*  RETURNS: n/a
*
*  FUNCTION: Generate C code from tables.
*
*  NOTES: All inputs are external globals.
*-*************************************************************************/
phase2()
{
	extern	Var	*var_list;	/* variables (from parse) */
	extern	Expr	*ss_list;	/* state sets (from parse) */
	extern	Expr	*global_c_list;	/* global C code */

	/* Count number of db channels and state sets defined */
	num_channels = db_chan_count();
	num_ss = exprCount(ss_list);

	/* Reconcile all variable and tie each to the appropriate VAR struct */
	reconcile_variables();

	/* reconcile all state names, including next state in transitions */
	reconcile_states();

	/* Generate preamble code */
	gen_preamble();

	/* Generate variable declarations */
	gen_var_decl();

	/* Generate definition C code */
	gen_defn_c_code();

	/* Assign bits for event flags */
	assign_ef_bits();

	/* Assign delay id's */
	assign_delay_ids();

	/* Generate code for each state set */
	gen_ss_code();

	/* Generate tables */
	gen_tables();

	/* Output global C code */
	gen_global_c_code();

	exit(0);
}
/* Generate preamble (includes, defines, etc.) */
gen_preamble()
{
	extern char		*prog_name;
	extern int		async_opt, conn_opt, debug_opt, reent_opt;

	/* Program name (comment) */
	printf("/* Program \"%s\" */\n", prog_name);

	/* Include files */
	printf("#include \"seq.h\"\n");

	/* Local definitions */
	printf("\n#define NUM_SS %d\n", num_ss);
	printf("#define NUM_CHANNELS %d\n", num_channels);

	/* #define's for compiler options */
	gen_opt_defn(async_opt, "ASYNC_OPT");
	gen_opt_defn(conn_opt,  "CONN_OPT" );
	gen_opt_defn(debug_opt, "DEBUG_OPT");
	gen_opt_defn(reent_opt, "REENT_OPT");
	printf("\n");

	/* Forward references of tables: */
	printf("\nextern SPROG %s;\n", prog_name);
	printf("extern CHAN db_channels[];\n");

	return;
}

gen_opt_defn(opt, defn_name)
int		opt;
char		*defn_name;
{
	if (opt)
		printf("#define %s TRUE\n", defn_name);
	else
		printf("#define %s FALSE\n", defn_name);
}

/* Reconcile all variables in an expression,
 and tie each to the appropriate VAR struct */
int	printTree = FALSE;

reconcile_variables()
{
	extern Expr		*ss_list, *exit_code_list;
	Expr			*ssp, *ep;
	int			connect_variable();

	for (ssp = ss_list; ssp != 0; ssp = ssp->next)
	{
#ifdef	DEBUG
		fprintf(stderr, "reconcile_variables: ss=%s\n", ssp->value);
#endif	/* DEBUG */
		traverseExprTree(ssp, E_VAR, 0, connect_variable, 0);
	}

	/* Same for exit procedure */
	for (ep = exit_code_list; ep != 0; ep = ep->next)
	{
		traverseExprTree(ep, E_VAR, 0, connect_variable, 0);
	}

}

/* Connect a variable in an expression to the the Var structure */
int connect_variable(ep)
Expr		*ep;
{
	Var		*vp;
	extern char	*stype[];
	extern int	warn_opt;

	vp = (Var *)findVar(ep->value);
	if (vp == 0)
	{	/* variable not declared; add it to the variable list */
		if (warn_opt)
			fprintf(stderr,
			 "Warning:  variable \"%s\" is used but not declared.\n",
			 ep->value);
		vp = allocVar();
		addVar(vp);
		vp->name = ep->value;
		vp->type = V_NONE; /* undeclared type */
		vp->length = 0;
		vp->value = 0;
	}
	ep->left = (Expr *)vp; /* make connection */
#ifdef	DEBUG
	fprintf(stderr, "connect_variable: %s\n", ep->value);
#endif
	return;
} 

/* Reconcile state names */
reconcile_states()
{
	extern int	num_errors;
	extern  Expr    *ss_list;
	Expr		*ssp, *sp, *sp1, tr;

	for (ssp = ss_list; ssp != 0; ssp = ssp->next)
	{
		for (sp = ssp->left; sp != 0; sp = sp->next)
		{
			/* Check for duplicate state names in this state set */
			for (sp1 = sp->next; sp1 != 0; sp1 = sp1->next)
			{
				if (strcmp(sp->value, sp1->value) == 0)
				{
					fprintf(stderr,
			"State \"%s\" is duplicated in state set \"%s\"\n",
				     sp->value, ssp->value);
					num_errors++;
				}
			}		
		}
	}
}

/* Find a state by name */
Expr *find_state(name, sp)
char		*name;	/* state name */
Expr		*sp;	/* beginning of state list */
{
	while (sp != 0)
	{
		if (strcmp(name, sp->value) == 0)
			return sp;
		sp = sp->next;
	}
	return 0;
}


/* Generate a C variable declaration for each variable declared in SNL */
gen_var_decl()
{
	extern Var	*var_list;
	Var		*vp;
	char		*vstr;
	int		nv;
	extern int	reent_opt;

	printf("\n/* Variable declarations */\n");

	/* Convert internal type to `C' type */
	if (reent_opt)
		printf("struct UserVar {\n");
	for (nv=0, vp = var_list; vp != NULL; nv++, vp = vp->next)
	{
		switch (vp->type)
		{
		case V_CHAR:
			vstr = "char";
			break;
		case V_INT:
		case V_LONG:
			vstr = "long";
			break;
		case V_SHORT:
			vstr = "short";
			break;
		case V_FLOAT:
			vstr = "float";
			break;
 		case V_DOUBLE:
			vstr = "double";
			break;
		case V_STRING:
			vstr = "char";
			break;
		case V_EVFLAG:
		case V_NONE:
			vstr = NULL;
			break;
		default:
			vstr = "int";
			break;
		}

		if (vstr != NULL)
		{
			if (reent_opt)
				printf("\t");
			else
				printf("static ");
			printf("%s\t%s", vstr, vp->name);
			if (vp->length > 0)
				printf("[%d]", vp->length);
			else if (vp->type == V_STRING)
				printf("[MAX_STRING_SIZE]");
			printf(";\n");
		}
	}
	if (reent_opt)
		printf("};\n");
	return;
}		

/* Generate definition C code (C code in definition section) */
gen_defn_c_code()
{
	extern Expr	*defn_c_list;
	Expr		*ep;

	ep = defn_c_list;
	if (ep != NULL)
	{
		printf("\n\t/* C code definitions */\n");
		for (; ep != NULL; ep = ep->next)
		{
			print_line_num(ep->line_num, ep->src_file);
			printf("%s\n", ep->left);
		}
	}
	return;
}
/* Generate global C code (C code following state program) */
gen_global_c_code()
{
	extern Expr	*global_c_list;
	Expr		*ep;

	ep = global_c_list;
	if (ep != NULL)
	{
		printf("\f\t/* Global C code */\n");
		for (; ep != NULL; ep = ep->next)
		{
			print_line_num(ep->line_num, ep->src_file);
			printf("%s\n", ep->left);
		}
	}
	return;
}

/* Returns number of db channels defined & inserts index into each channel struct */
db_chan_count()
{
	extern	Chan	*chan_list;
	int	nchan;
	Chan	*cp;

	nchan = 0;
	for (cp = chan_list; cp != NULL; cp = cp->next)
	{
		if (cp->db_name != NULL)
		{
			cp->index = nchan;
			nchan++;
#ifdef	DEBUG
			fprintf(stderr, "db_name=%s, index=%d\n",
			 cp->db_name, cp->index);
#endif
		}
	}
	return nchan;
}


/* Assign bits to event flags and database variables */
assign_ef_bits()
{
	extern Var		*var_list;
	Var			*vp;
	Chan			*cp;
	int			ef_num;
	extern int		num_channels;

	ef_num = num_channels + 1; /* start with 1-st avail mask bit */
	printf("\n/* Event flags */\n");
#ifdef	DEBUG
	fprintf(stderr, "\nAssign values to event flags\n");
#endif
	for (vp = var_list; vp != NULL; vp = vp->next)
	{
		cp = vp->chan;
		/* First see if this is an event flag  */
		if (vp->type == V_EVFLAG)
		{
			if (cp != 0)
				vp->ef_num = cp->index + 1; /* sync'ed */
			else
				vp->ef_num = ef_num++;
			printf("#define %s\t%d\n", vp->name, vp->ef_num);
		}

		else
		{
			if (cp != 0)
				vp->ef_num = cp->index + 1;
		}

#ifdef	DEBUG
		fprintf(stderr, "%s: ef_num=%d\n", vp->name, vp->ef_num);
#endif
	}
	return;
}

/* Assign a delay id to each "delay()" in an event (when()) expression */
assign_delay_ids()
{
	extern Expr		*ss_list;
	Expr			*ssp, *sp, *tp;
	int			delay_id;
	int			assign_next_delay_id();

	for (ssp = ss_list; ssp != 0; ssp = ssp->next)
	{
		for (sp = ssp->left; sp != 0; sp = sp->next)
		{
			/* Each state has it's own delay id's */
			delay_id = 0;
			for (tp = sp->left; tp != 0; tp = tp->next)
			{	/* traverse event expression only */
				traverseExprTree(tp->left, E_FUNC, "delay",
				 assign_next_delay_id, &delay_id);
			}
		}
	}
}

assign_next_delay_id(ep, delay_id)
Expr			*ep;
int			*delay_id;
{
	ep->right = (Expr *)*delay_id;
	*delay_id += 1;
}

/* Traverse the expression tree, and call the supplied
 * function on matched conditions */
traverseExprTree(ep, type, value, funcp, argp)
Expr		*ep;		/* ptr to start of expression */
int		type;		/* to search for */
char		*value;		/* with optional matching value */
void		(*funcp)();	/* function to call */
void		*argp;		/* ptr to argument to pass on to function */
{
	Expr		*ep1;
	extern char	*stype[];

	if (ep == 0)
		return;

	if (printTree)
		fprintf(stderr, "traverseExprTree: type=%s, value=%s\n",
		 stype[ep->type], ep->value);

	/* Call the function? */
	if ((ep->type == type) &&
		(value == 0 || strcmp(ep->value, value) == 0) )
	{
		funcp(ep, argp);
	}

	/* Continue traversing the expression tree */
	switch(ep->type)
	{
	case E_EMPTY:
	case E_VAR:
	case E_CONST:
	case E_STRING:
	case E_TEXT:
	case E_BREAK:
		break;

	case E_PAREN:
	case E_UNOP:
	case E_SS:
	case E_STATE:
	case E_FUNC:
	case E_CMPND:
	case E_STMT:
	case E_ELSE:
	case E_PRE:
	case E_POST:
		for (ep1 = ep->left; ep1 != 0;	ep1 = ep1->next)
		{
			traverseExprTree(ep1, type, value, funcp, argp);
		}
		break;

	case E_WHEN:
	case E_ASGNOP:
	case E_BINOP:
	case E_SUBSCR:
	case E_IF:
	case E_WHILE:
	case E_FOR:
	case E_X:
		for (ep1 = ep->left; ep1 != 0;	ep1 = ep1->next)
		{
			traverseExprTree(ep1, type, value, funcp, argp);
		}
		for (ep1 = ep->right; ep1 != 0;	ep1 = ep1->next)
		{
			traverseExprTree(ep1, type, value, funcp, argp);
		}
		break;

	default:
		fprintf(stderr, "traverseExprTree: type=%d???\n", ep->type);
	}
}

