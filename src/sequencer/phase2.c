/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

 	phase2.c,v 1.2 1995/06/27 15:25:52 wright Exp

	DESCRIPTION: Phase 2 code generation routines for SNC.
	Produces code and tables in C output file.
	See also:  gen_ss_code.c and gen_tables.c
	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Replaced lstLib calls with built-in linked list.
19nov91,ajk	Removed extraneous "static" from "UserVar" declaration.
01mar94,ajk	Implemented new interface to sequencer (seqCom.h).
01mar94,ajk	Implemented assignment of array elements to db channels.
01mar94,ajk	Changed algorithm for assigning event bits.
13jan98,wfl	Supported E_COMMA token (for compound expressions).
09mar98,wfl	Avoided compilation warnings under Tornado
***************************************************************************/
/*#define	DEBUG	1*/

#include	<stdio.h>
#include	"parse.h"
#include 	<seqCom.h>

int	num_channels = 0;	/* number of db channels */
int	num_events = 0;		/* number of event flags */
int	num_ss = 0;		/* number of state sets */
int	max_delays = 0;		/* maximum number of delays per state */
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
*  FUNCTION: Generate C code from parsing lists.
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

	/* Assign bits for event flags */
	assign_ef_bits();

	/* Assign delay id's */
	assign_delay_ids();

	/* Generate preamble code */
	gen_preamble();

	/* Generate variable declarations */
	gen_var_decl();

	/* Generate definition C code */
	gen_defn_c_code();

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
	printf("#define ANSI\n"); /* Expands ANSI prototypes in seqCom.h */
	printf("#include \"seqCom.h\"\n");

	/* Local definitions */
	printf("\n#define NUM_SS %d\n", num_ss);
	printf("#define NUM_CHANNELS %d\n", num_channels);
	printf("#define NUM_EVENTS %d\n", num_events);

	/* The following definition should be consistant with db_access.h */
	printf("#define MAX_STRING_SIZE 40\n");

	/* #define's for compiler options */
	gen_opt_defn(async_opt, "ASYNC_OPT");
	gen_opt_defn(conn_opt,  "CONN_OPT" );
	gen_opt_defn(debug_opt, "DEBUG_OPT");
	gen_opt_defn(reent_opt, "REENT_OPT");
	printf("\n");

	/* Forward references of tables: */
	printf("\nextern struct seqProgram %s;\n", prog_name);

	return;
}

/* Generate defines for compiler options */
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
 * and tie each to the appropriate VAR structure.
 */
int	printTree = FALSE; /* For debugging only */

reconcile_variables()
{
	extern Expr		*ss_list, *exit_code_list;
	Expr			*ssp, *ep;
	int			connect_variable();

	for (ssp = ss_list; ssp != 0; ssp = ssp->next)
	{
#ifdef	DEBUG
		fprintf(stderr, "reconcile_variables: ss=%s\n", ssp->value);
#endif	/*DEBUG*/
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

	if (ep->type != E_VAR)
		return;
#ifdef	DEBUG
	fprintf(stderr, "connect_variable: \"%s\", line %d\n", ep->value, ep->line_num);
#endif
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
		vp->length1 = 1;
		vp->length2 = 1;
		vp->value = 0;
	}
	ep->left = (Expr *)vp; /* make connection */
	return;
} 

/* Reconcile state names */
reconcile_states()
{

	extern Expr		*ss_list;

	extern int	num_errors;
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
			vstr = "int";
			break;
		  case V_LONG:
			vstr = "long";
			break;
		  case V_SHORT:
			vstr = "short";
			break;
		  case V_UCHAR:
			vstr = "unsigned char";
			break;
		  case V_UINT:
			vstr = "unsigned int";
			break;
		  case V_ULONG:
			vstr = "unsigned long";
			break;
		  case V_USHORT:
			vstr = "unsigned short";
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
		if (vstr == NULL)
			continue;

		if (reent_opt)
			printf("\t");
		else
			printf("static ");

		printf("%s\t", vstr);

		if (vp->class == VC_POINTER || vp->class == VC_ARRAYP)
			printf("*");

		printf("%s", vp->name);

		if (vp->class == VC_ARRAY1 || vp->class == VC_ARRAYP)
			printf("[%d]", vp->length1);

		else if (vp->class == VC_ARRAY2)
			printf("[%d][%d]", vp->length1, vp->length2);

		if (vp->type == V_STRING)
			printf("[MAX_STRING_SIZE]");

		printf(";\n");
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
		print_line_num(ep->line_num, ep->src_file);
		for (; ep != NULL; ep = ep->next)
		{
			printf("%s\n", ep->left);
		}
	}
	return;
}

/* Sets cp->index for each variable, & returns number of db channels defined. 
 */
db_chan_count()
{
	extern	Chan	*chan_list;
	int	nchan;
	Chan	*cp;

	nchan = 0;
	for (cp = chan_list; cp != NULL; cp = cp->next)
	{
		cp->index = nchan;
		if (cp->num_elem == 0)
			nchan += 1;
		else
			nchan += cp->num_elem; /* array with multiple channels */
	}

	return nchan;
}


/* Assign event bits to event flags and associate db channels with
 * event flags.
 */
assign_ef_bits()
{
	extern Var	*var_list;
	extern	Chan	*chan_list;
	Var		*vp;
	Chan		*cp;
	extern int	num_events;
	int		n;

	/* Assign event flag numbers (starting at 1) */
	printf("\n/* Event flags */\n");
	num_events = 0;
	for (vp = var_list; vp != NULL; vp = vp->next)
	{
		if (vp->type == V_EVFLAG)
		{
			num_events++;
			vp->ef_num = num_events;
			printf("#define %s\t%d\n", vp->name, num_events);
		}
	}
			
	/* Associate event flags with DB channels */
	for (cp = chan_list; cp != NULL; cp = cp->next)
	{
		if (cp->num_elem == 0)
		{
			if (cp->ef_var != NULL)
			{
				vp = cp->ef_var;
				cp->ef_num = vp->ef_num;
			}
		}

		else /* cp->num_elem != 0 */
		{
			for (n = 0; n < cp->num_elem; n++)
			{
			    vp = cp->ef_var_list[n];
			    if (vp != NULL)
			    {
				cp->ef_num_list[n] = vp->ef_num;
			    }
			}
		}

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

#ifdef	DEBUG
	fprintf(stderr, "assign_delay_ids:\n");
#endif	/*DEBUG*/
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

			/* Keep track of number of delay id's requied */
			if (delay_id > max_delays)
				max_delays = delay_id;
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
 * function whenever type = ep->type AND value matches ep->value.
 * The condition value = 0 matches all.
 * The function is called with the current ep and a supplied argument (argp) */
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
	if ((ep->type == type) && (value == 0 || strcmp(ep->value, value) == 0) )
	{
		funcp(ep, argp);
	}

	/* Continue traversing the expression tree */
	switch(ep->type)
	{
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
	case E_COMMA:
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

